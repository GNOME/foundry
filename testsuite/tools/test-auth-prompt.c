/* test-auth-prompt.c
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

#include <foundry.h>

static DexFuture *
main_fiber (gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FoundryAuthPromptBuilder) builder = NULL;
  g_autoptr(FoundryAuthPrompt) prompt = NULL;
  g_autoptr(FoundryContext) context = NULL;
  GMainLoop *main_loop = user_data;
  g_autofree char *username = NULL;
  g_autofree char *password = NULL;
  gboolean r;

  r = dex_await (foundry_init (), &error);
  g_assert_no_error (error);
  g_assert_true (r);

  context = dex_await_object (foundry_context_new_for_user (NULL), &error);
  g_assert_no_error (error);
  g_assert_nonnull (context);
  g_assert_true (FOUNDRY_IS_CONTEXT (context));

  builder = foundry_auth_prompt_builder_new (context);
  foundry_auth_prompt_builder_set_title (builder, "Test Auth Prompt");
  foundry_auth_prompt_builder_set_subtitle (builder, "Subtitle for auth prompt");
  foundry_auth_prompt_builder_add_param (builder, "username", "Username", NULL, FALSE);
  foundry_auth_prompt_builder_add_param (builder, "password", "Password", NULL, TRUE);

  prompt = foundry_auth_prompt_builder_end (builder);

  r = dex_await (foundry_auth_prompt_query (prompt), &error);
  g_assert_no_error (error);
  g_assert_true (r);

  username = foundry_auth_prompt_dup_prompt_value (prompt, "username");
  password = foundry_auth_prompt_dup_prompt_value (prompt, "password");

  g_print ("\n");
  g_print ("Username was `%s`\n", username);
  g_print ("Password was `%s`\n", password);

  g_main_loop_quit (main_loop);

  return dex_future_new_true ();
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);

  dex_init ();

  dex_future_disown (dex_scheduler_spawn (NULL, 0, main_fiber,
                                          g_main_loop_ref (main_loop),
                                          (GDestroyNotify) g_main_loop_unref));

  g_main_loop_run (main_loop);

  return 0;
}
