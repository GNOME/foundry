/* foundry-auth-provider.c
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

#include "foundry-auth-provider.h"
#include "foundry-util.h"

G_DEFINE_ABSTRACT_TYPE (FoundryAuthProvider, foundry_auth_provider, FOUNDRY_TYPE_CONTEXTUAL)

static void
foundry_auth_provider_class_init (FoundryAuthProviderClass *klass)
{
}

static void
foundry_auth_provider_init (FoundryAuthProvider *self)
{
}

/**
 * foundry_auth_provider_prompt:
 * @self: a [class@Foundry.AuthProvider]
 * @prompt: a [class@Foundry.AuthPrompt]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   when the prompt has been completed by the user.
 */
DexFuture *
foundry_auth_provider_prompt (FoundryAuthProvider *self,
                              FoundryAuthPrompt   *prompt)
{
  dex_return_error_if_fail (FOUNDRY_IS_AUTH_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_AUTH_PROMPT (prompt));

  if (FOUNDRY_AUTH_PROVIDER_GET_CLASS (self)->prompt)
    return FOUNDRY_AUTH_PROVIDER_GET_CLASS (self)->prompt (self, prompt);

  return foundry_future_new_not_supported ();
}
