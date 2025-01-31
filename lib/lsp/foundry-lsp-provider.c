/* foundry-lsp-provider.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>

#include "foundry-lsp-provider-private.h"
#include "foundry-config-private.h"

G_DEFINE_ABSTRACT_TYPE (FoundryLspProvider, foundry_lsp_provider, FOUNDRY_TYPE_CONTEXTUAL)

static DexFuture *
foundry_lsp_provider_real_load (FoundryLspProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_lsp_provider_real_unload (FoundryLspProvider *self)
{
  return dex_future_new_true ();
}

static void
foundry_lsp_provider_class_init (FoundryLspProviderClass *klass)
{
  klass->load = foundry_lsp_provider_real_load;
  klass->unload = foundry_lsp_provider_real_unload;
}

static void
foundry_lsp_provider_init (FoundryLspProvider *self)
{
}

/**
 * foundry_lsp_provider_load:
 * @self: a #FoundryLspProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_lsp_provider_load (FoundryLspProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LSP_PROVIDER (self), NULL);

  return FOUNDRY_LSP_PROVIDER_GET_CLASS (self)->load (self);
}

/**
 * foundry_lsp_provider_unload:
 * @self: a #FoundryLspProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_lsp_provider_unload (FoundryLspProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LSP_PROVIDER (self), NULL);

  return FOUNDRY_LSP_PROVIDER_GET_CLASS (self)->unload (self);
}

/**
 * foundry_lsp_provider_dup_name:
 * @self: a #FoundryLspProvider
 *
 * Gets a name for the provider that is expected to be displayed to
 * users such as "Flatpak".
 *
 * Returns: (transfer full): the name of the provider
 */
char *
foundry_lsp_provider_dup_name (FoundryLspProvider *self)
{
  char *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_LSP_PROVIDER (self), NULL);

  if (FOUNDRY_LSP_PROVIDER_GET_CLASS (self)->dup_name)
    ret = FOUNDRY_LSP_PROVIDER_GET_CLASS (self)->dup_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return g_steal_pointer (&ret);
}

/**
 * foundry_lsp_provider_spawn:
 * @self: a #FoundryLspProvider
 *
 * Attempts to spawn the language server which can communicate
 * over `stdin`/`stdout`.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to a [class@Gio.Subprocess].
 */
DexFuture *
foundry_lsp_provider_spawn (FoundryLspProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_PROVIDER (self));

  return FOUNDRY_LSP_PROVIDER_GET_CLASS (self)->spawn (self);
}
