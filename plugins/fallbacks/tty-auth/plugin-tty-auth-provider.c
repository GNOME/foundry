/* plugin-tty-auth-provider.c
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

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "plugin-tty-auth-provider.h"

struct _PluginTtyAuthProvider
{
  FoundryAuthProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginTtyAuthProvider, plugin_tty_auth_provider, FOUNDRY_TYPE_AUTH_PROVIDER)

static gboolean
read_password (const char *prompt,
               char       *buf,
               size_t      buflen)
{
  struct termios oldt, newt;
  int i = 0;
  int c;

  g_assert (prompt != NULL);
  g_assert (buf != NULL);
  g_assert (buflen > 0);

  fprintf (stderr, "\033[1m%s\033[0m: ", prompt);
  fflush (stderr);

  if (tcgetattr (STDIN_FILENO, &oldt) != 0)
    return FALSE;

  newt = oldt;
  newt.c_lflag &= ~ECHO;

  if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &newt) != 0)
    return FALSE;

  while ((c = getchar ()) != '\n' && c != EOF && i < buflen - 1)
    buf[i++] = c;
  buf[i] = '\0';

  tcsetattr (STDIN_FILENO, TCSAFLUSH, &oldt);

  fprintf (stderr, "\n");

  return FALSE;
}

static gboolean
read_entry (const char *prompt,
            char       *buf,
            size_t      buflen)
{
  int i = 0;
  int c;

  g_assert (prompt != NULL);
  g_assert (buf != NULL);
  g_assert (buflen > 0);

  fprintf (stderr, "\033[1m%s\033[0m: ", prompt);
  fflush (stderr);

  while ((c = getchar ()) != '\n' && c != EOF && i < buflen - 1)
    buf[i++] = c;
  buf[i] = '\0';

  return FALSE;
}

static DexFuture *
plugin_tty_auth_provider_prompt_func (gpointer data)
{
  FoundryAuthPrompt *prompt = data;
  g_auto(GStrv) prompts = NULL;
  g_autofree char *title = NULL;
  g_autofree char *subtitle = NULL;

  g_assert (FOUNDRY_IS_AUTH_PROMPT (prompt));

  prompts = foundry_auth_prompt_dup_prompts (prompt);
  title = foundry_auth_prompt_dup_title (prompt);
  subtitle = foundry_auth_prompt_dup_subtitle (prompt);

  if (title != NULL)
    g_print ("\033[1m%s\033[0m\n", title);

  if (subtitle != NULL)
    g_print ("\033[3m%s\033[23m\n", subtitle);

  g_print ("\n");

  for (guint i = 0; prompts[i]; i++)
    {
      g_autofree char *pname = foundry_auth_prompt_dup_prompt_name (prompt, prompts[i]);
      char value[512];

      if (foundry_auth_prompt_is_prompt_hidden (prompt, prompts[i]))
        read_password (pname, value, sizeof value);
      else
        read_entry (pname, value, sizeof value);
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_tty_auth_provider_prompt (FoundryAuthProvider *auth_provider,
                                 FoundryAuthPrompt   *prompt)
{
  g_assert (PLUGIN_IS_TTY_AUTH_PROVIDER (auth_provider));
  g_assert (FOUNDRY_IS_AUTH_PROMPT (prompt));

  if (!isatty (STDIN_FILENO) || !isatty (STDOUT_FILENO))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "stdin/stdout must be TTY");

  return dex_thread_spawn ("[dex-auth-thread]",
                           plugin_tty_auth_provider_prompt_func,
                           g_object_ref (prompt),
                           g_object_unref);
}

static void
plugin_tty_auth_provider_class_init (PluginTtyAuthProviderClass *klass)
{
  FoundryAuthProviderClass *auth_provider_class = FOUNDRY_AUTH_PROVIDER_CLASS (klass);

  auth_provider_class->prompt = plugin_tty_auth_provider_prompt;
}

static void
plugin_tty_auth_provider_init (PluginTtyAuthProvider *self)
{
}
