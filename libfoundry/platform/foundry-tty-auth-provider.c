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

#include "foundry-input-group.h"
#include "foundry-input-password.h"
#include "foundry-input-text.h"
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
  FoundryInput *prompt;
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

static void
print_title (FoundryTtyAuthProvider *self,
             FoundryInput           *input)
{
  g_autofree char *title = foundry_input_dup_title (input);

  if (title != NULL)
    fd_printf (self->pty_fd, "\033[1m%s\033[0m\n", title);
}

static void
print_subtitle (FoundryTtyAuthProvider *self,
                FoundryInput           *input)
{
  g_autofree char *subtitle = foundry_input_dup_subtitle (input);

  if (subtitle != NULL)
    fd_printf (self->pty_fd, "\033[1m%s\033[0m\n", subtitle);
}

static gboolean
foundry_tty_auth_provider_prompt_recurse (FoundryTtyAuthProvider *self,
                                          FoundryInput           *input)
{
  g_assert (FOUNDRY_IS_TTY_AUTH_PROVIDER (self));
  g_assert (FOUNDRY_IS_INPUT (input));

  if (FOUNDRY_IS_INPUT_GROUP (input))
    {
      GListModel *model;
      guint n_items;

      print_title (self, input);
      print_subtitle (self, input);

      fd_printf (self->pty_fd, "\n");

      model = foundry_input_group_list_children (FOUNDRY_INPUT_GROUP (input));
      n_items = g_list_model_get_n_items (model);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(FoundryInput) child = g_list_model_get_item (model, i);

          if (!foundry_tty_auth_provider_prompt_recurse (self, child))
            return FALSE;
        }

      return TRUE;
    }
  else if (FOUNDRY_IS_INPUT_TEXT (input))
    {
      g_autofree char *title = foundry_input_dup_title (input);
      char value[512];

      if (read_entry (self, title, value, sizeof value))
        {
          value[G_N_ELEMENTS (value)-1] = 0;
          foundry_input_text_set_value (FOUNDRY_INPUT_TEXT (input), value);
          return TRUE;
        }
    }
  else if (FOUNDRY_IS_INPUT_PASSWORD (input))
    {
      g_autofree char *title = foundry_input_dup_title (input);
      char value[512];

      if (read_password (self, title, value, sizeof value))
        {
          value[G_N_ELEMENTS (value)-1] = 0;
          foundry_input_password_set_value (FOUNDRY_INPUT_PASSWORD (input), value);
          return TRUE;
        }
    }

  return FALSE;
}

static DexFuture *
foundry_tty_auth_provider_prompt_thread (gpointer data)
{
  Prompt *state = data;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_INPUT (state->prompt));
  g_assert (FOUNDRY_IS_TTY_AUTH_PROVIDER (state->self));

  if (!foundry_tty_auth_provider_prompt_recurse (state->self, state->prompt))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Failed");

  return dex_future_new_true ();
}

static DexFuture *
foundry_tty_auth_provider_prompt (FoundryAuthProvider *auth_provider,
                                  FoundryInput        *prompt)
{
  FoundryTtyAuthProvider *self = (FoundryTtyAuthProvider *)auth_provider;
  Prompt *state;

  g_assert (FOUNDRY_IS_TTY_AUTH_PROVIDER (self));
  g_assert (FOUNDRY_IS_INPUT (prompt));

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
