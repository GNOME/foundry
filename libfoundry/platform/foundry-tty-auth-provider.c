/* foundry-tty-auth-provider.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <termios.h>
#include <unistd.h>

#include <glib/gstdio.h>

#include "foundry-tty-auth-provider.h"

struct _FoundryTtyAuthProvider
{
  FoundryAuthProvider parent_instance;
  int pty_fd;
};

G_DEFINE_FINAL_TYPE (FoundryTtyAuthProvider, foundry_tty_auth_provider, FOUNDRY_TYPE_AUTH_PROVIDER)

typedef struct _Prompt
{
  FoundryTtyAuthProvider *self;
  FoundryAuthPrompt *prompt;
} Prompt;

static void
prompt_free (Prompt *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->prompt);
  g_free (state);
}

G_GNUC_PRINTF (2, 3)
static void
fd_printf (int         fd,
           const char *format,
           ...)
{
  g_autofree char *formatted = NULL;
  va_list args;
  gssize len;

  va_start (args, format);
  len = g_vasprintf (&formatted, format, args);
  va_end (args);

  if (len < 0)
    return;

  (void)write (fd, formatted, len);
}

static int
fd_getchar (int fd)
{
  char c;

  switch (read (fd, &c, 1))
    {
    case 0: return EOF;
    case 1: return c;
    default: return -1;
    }
}

static gboolean
read_password (FoundryTtyAuthProvider *self,
               const char             *prompt,
               char                   *buf,
               size_t                  buflen)
{
  struct termios oldt, newt;
  int i = 0;
  int c;

  g_assert (FOUNDRY_IS_TTY_AUTH_PROVIDER (self));
  g_assert (prompt != NULL);
  g_assert (buf != NULL);
  g_assert (buflen > 0);

  fd_printf (self->pty_fd, "\033[1m%s\033[0m: ", prompt);

  if (tcgetattr (self->pty_fd, &oldt) != 0)
    return FALSE;

  newt = oldt;
  newt.c_lflag &= ~ECHO;

  if (tcsetattr (self->pty_fd, TCSAFLUSH, &newt) != 0)
    return FALSE;

  while ((c = fd_getchar (self->pty_fd)) != '\n' && c != EOF && i < buflen - 1)
    buf[i++] = c;
  buf[i] = '\0';

  tcsetattr (self->pty_fd, TCSAFLUSH, &oldt);

  fd_printf (self->pty_fd, "\n");

  return TRUE;
}

static gboolean
read_entry (FoundryTtyAuthProvider *self,
            const char             *prompt,
            char                   *buf,
            size_t                  buflen)
{
  int i = 0;
  int c;

  g_assert (FOUNDRY_IS_TTY_AUTH_PROVIDER (self));
  g_assert (prompt != NULL);
  g_assert (buf != NULL);
  g_assert (buflen > 0);

  fd_printf (self->pty_fd, "\033[1m%s\033[0m: ", prompt);

  while ((c = fd_getchar (self->pty_fd)) != '\n' && c != EOF && i < buflen - 1)
    buf[i++] = c;
  buf[i] = '\0';

  return TRUE;
}

static DexFuture *
foundry_tty_auth_provider_prompt_thread (gpointer data)
{
  Prompt *state = data;
  FoundryTtyAuthProvider *self;
  FoundryAuthPrompt *prompt;
  g_auto(GStrv) prompts = NULL;
  g_autofree char *title = NULL;
  g_autofree char *subtitle = NULL;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_AUTH_PROMPT (state->prompt));
  g_assert (FOUNDRY_IS_TTY_AUTH_PROVIDER (state->self));

  self = state->self;
  prompt = state->prompt;

  prompts = foundry_auth_prompt_dup_prompts (prompt);
  title = foundry_auth_prompt_dup_title (prompt);
  subtitle = foundry_auth_prompt_dup_subtitle (prompt);

  if (title != NULL)
    fd_printf (self->pty_fd, "\033[1m%s\033[0m\n", title);

  if (subtitle != NULL)
    fd_printf (self->pty_fd, "\033[3m%s\033[23m\n", subtitle);

  fd_printf (self->pty_fd, "\n");

  for (guint i = 0; prompts[i]; i++)
    {
      g_autofree char *pname = foundry_auth_prompt_dup_prompt_name (prompt, prompts[i]);
      gboolean ret;
      char value[512];

      if (foundry_auth_prompt_is_prompt_hidden (prompt, prompts[i]))
        ret = read_password (self, pname, value, sizeof value);
      else
        ret = read_entry (self, pname, value, sizeof value);

      if (ret)
        foundry_auth_prompt_set_prompt_value (prompt, prompts[i], value);
    }

  return dex_future_new_true ();
}

static DexFuture *
foundry_tty_auth_provider_prompt (FoundryAuthProvider *auth_provider,
                                  FoundryAuthPrompt   *prompt)
{
  FoundryTtyAuthProvider *self = (FoundryTtyAuthProvider *)auth_provider;
  Prompt *state;

  g_assert (FOUNDRY_IS_TTY_AUTH_PROVIDER (self));
  g_assert (FOUNDRY_IS_AUTH_PROMPT (prompt));

  if (self->pty_fd == -1)
    return dex_future_new_reject (G_FILE_ERROR,
                                  G_FILE_ERROR_BADF,
                                  "%s",
                                  g_strerror (EBADF));

  state = g_new0 (Prompt, 1);
  state->self = g_object_ref (self);
  state->prompt = g_object_ref (prompt);

  return dex_thread_spawn ("[dex-auth-thread]",
                           foundry_tty_auth_provider_prompt_thread,
                           state,
                           (GDestroyNotify) prompt_free);
}

static void
foundry_tty_auth_provider_finalize (GObject *object)
{
  FoundryTtyAuthProvider *self = (FoundryTtyAuthProvider *)object;

  g_clear_fd (&self->pty_fd, NULL);

  G_OBJECT_CLASS (foundry_tty_auth_provider_parent_class)->finalize (object);
}

static void
foundry_tty_auth_provider_class_init (FoundryTtyAuthProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryAuthProviderClass *auth_provider_class = FOUNDRY_AUTH_PROVIDER_CLASS (klass);

  object_class->finalize = foundry_tty_auth_provider_finalize;

  auth_provider_class->prompt = foundry_tty_auth_provider_prompt;
}

static void
foundry_tty_auth_provider_init (FoundryTtyAuthProvider *self)
{
  self->pty_fd = -1;
}

FoundryAuthProvider *
foundry_tty_auth_provider_new (int pty_fd)
{
  g_return_val_if_fail (pty_fd > -1, NULL);
  g_return_val_if_fail (isatty (pty_fd), NULL);

  pty_fd = dup (pty_fd);

  if (pty_fd > -1)
    {
      FoundryTtyAuthProvider *self = g_object_new (FOUNDRY_TYPE_TTY_AUTH_PROVIDER, NULL);
      self->pty_fd = pty_fd;
      return FOUNDRY_AUTH_PROVIDER (self);
    }

  return NULL;
}
