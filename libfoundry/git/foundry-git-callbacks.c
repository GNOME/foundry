/* foundry-git-callbacks.c
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

#include <glib/gi18n-lib.h>

#include <libssh2.h>

#include "foundry-git-callbacks-private.h"
#include "foundry-operation.h"

typedef struct _CallbackState
{
  FoundryAuthProvider *auth_provider;
  FoundryOperation    *operation;
  guint                tried;
} CallbackState;

static void
ssh_interactive_prompt (const char                            *name,
                        int                                    name_len,
                        const char                            *instruction,
                        int                                    instruction_len,
                        int                                    num_prompts,
                        const LIBSSH2_USERAUTH_KBDINT_PROMPT  *prompts,
                        LIBSSH2_USERAUTH_KBDINT_RESPONSE      *responses,
                        void                                 **abstract)
{
  CallbackState *state = *abstract;
  g_autoptr(FoundryAuthPromptBuilder) builder = NULL;
  g_autoptr(FoundryAuthPrompt) prompt = NULL;
  g_autofree char *instruction_copy = NULL;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_AUTH_PROVIDER (state->auth_provider));

  instruction_copy = g_strndup (instruction, instruction_len);

  builder = foundry_auth_prompt_builder_new (state->auth_provider);
  foundry_auth_prompt_builder_set_title (builder, instruction);

  for (int j = 0; j < num_prompts; j++)
    {
      const char *prompt_text = (const char *)prompts[j].text;
      gboolean hidden = !prompts[j].echo;

      foundry_auth_prompt_builder_add_param (builder, prompt_text, prompt_text, NULL, hidden);
    }

  prompt = foundry_auth_prompt_builder_end (builder);

  if (!dex_thread_wait_for (foundry_auth_prompt_query (prompt), NULL))
    return;

  for (int j = 0; j < num_prompts; j++)
    {
      const char *prompt_text = (const char *)prompts[j].text;
      g_autofree char *value = foundry_auth_prompt_dup_prompt_value (prompt, prompt_text);

      responses[j].text = strdup (value);
      responses[j].length = value ? strlen (value) : 0;
    }
}

static int
credentials_cb (git_cred     **out,
                const char    *url,
                const char    *username_from_url,
                unsigned int   allowed_types,
                void          *payload)
{
  CallbackState *state = payload;

  g_assert (state != NULL);
  g_assert (FOUNDRY_IS_AUTH_PROVIDER (state->auth_provider));

  allowed_types &= ~state->tried;

  if (allowed_types & GIT_CREDENTIAL_USERNAME)
    {
      state->tried |= GIT_CREDENTIAL_USERNAME;
      return git_cred_username_new (out,
                                    username_from_url ?
                                      username_from_url :
                                      g_get_user_name ());
    }

  if (allowed_types & GIT_CREDENTIAL_SSH_KEY)
    {
      state->tried |= GIT_CREDENTIAL_SSH_KEY;
      return git_cred_ssh_key_from_agent (out,
                                          username_from_url ?
                                            username_from_url :
                                            g_get_user_name ());
    }

  if (allowed_types & GIT_CREDENTIAL_DEFAULT)
    {
      state->tried |= GIT_CREDENTIAL_DEFAULT;
      return git_cred_default_new (out);
    }

  if (allowed_types & GIT_CREDENTIAL_SSH_INTERACTIVE)
    {
      g_autofree char *username = g_strdup (username_from_url);

      state->tried |= GIT_CREDENTIAL_SSH_INTERACTIVE;

      if (username == NULL)
        {
          g_autoptr(FoundryAuthPromptBuilder) builder = NULL;
          g_autoptr(FoundryAuthPrompt) prompt = NULL;

          builder = foundry_auth_prompt_builder_new (state->auth_provider);
          foundry_auth_prompt_builder_set_title (builder, _("Credentials"));
          foundry_auth_prompt_builder_add_param (builder,
                                                 "username",
                                                 _("Username"),
                                                 g_get_user_name (),
                                                 FALSE);

          prompt = foundry_auth_prompt_builder_end (builder);

          if (!dex_thread_wait_for (foundry_auth_prompt_query (prompt), NULL))
            return GIT_PASSTHROUGH;

          g_set_str (&username, foundry_auth_prompt_get_value (prompt, "username"));
        }

      return git_cred_ssh_interactive_new (out, username, ssh_interactive_prompt, state);
    }

  if (allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT)
    {
      g_autoptr(FoundryAuthPromptBuilder) builder = NULL;
      g_autoptr(FoundryAuthPrompt) prompt = NULL;

      state->tried |= GIT_CREDENTIAL_USERPASS_PLAINTEXT;

      builder = foundry_auth_prompt_builder_new (state->auth_provider);
      foundry_auth_prompt_builder_set_title (builder, _("Credentials"));
      foundry_auth_prompt_builder_add_param (builder,
                                             "username",
                                             _("Username"),
                                             username_from_url ? username_from_url : g_get_user_name (),
                                             FALSE);
      foundry_auth_prompt_builder_add_param (builder,
                                             "password",
                                             _("Password"),
                                             NULL,
                                             TRUE);

      prompt = foundry_auth_prompt_builder_end (builder);

      if (!dex_thread_wait_for (foundry_auth_prompt_query (prompt), NULL))
        return GIT_PASSTHROUGH;

      return git_cred_userpass_plaintext_new (out,
                                              foundry_auth_prompt_get_value (prompt, "username"),
                                              foundry_auth_prompt_get_value (prompt, "password"));
    }

  return GIT_PASSTHROUGH;
}

void
_foundry_git_callbacks_init (git_remote_callbacks *callbacks,
                             FoundryOperation     *operation,
                             FoundryAuthProvider  *auth_provider)
{
  CallbackState *state;

  g_return_if_fail (callbacks != NULL);
  g_return_if_fail (FOUNDRY_IS_OPERATION (operation));
  g_return_if_fail (FOUNDRY_IS_AUTH_PROVIDER (auth_provider));

  state = g_new0 (CallbackState, 1);
  state->auth_provider = g_object_ref (auth_provider);
  state->operation = g_object_ref (operation);
  state->tried = 0;

  git_remote_init_callbacks (callbacks, GIT_REMOTE_CALLBACKS_VERSION);

  callbacks->credentials = credentials_cb;
  callbacks->payload = state;
}

void
_foundry_git_callbacks_clear (git_remote_callbacks *callbacks)
{
  CallbackState *state;

  g_return_if_fail (callbacks != NULL);

  state = callbacks->payload;

  g_clear_object (&state->auth_provider);
  g_clear_object (&state->operation);
  g_free (state);
}
