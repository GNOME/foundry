/* foundry-completion-provider.c
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

#include "foundry-completion-provider-private.h"

G_DEFINE_ABSTRACT_TYPE (FoundryCompletionProvider, foundry_completion_provider, G_TYPE_OBJECT)

static DexFuture *
foundry_completion_provider_real_complete (FoundryCompletionProvider *self,
                                           FoundryCompletionRequest  *request)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (request));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static DexFuture *
foundry_completion_provider_real_refilter (FoundryCompletionProvider *self,
                                           FoundryCompletionRequest  *request,
                                           GListModel                *model)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (request));
  dex_return_error_if_fail (G_IS_LIST_MODEL (model));

  return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->complete (self, request);
}

static void
foundry_completion_provider_class_init (FoundryCompletionProviderClass *klass)
{
  klass->complete = foundry_completion_provider_real_complete;
  klass->refilter = foundry_completion_provider_real_refilter;
}

static void
foundry_completion_provider_init (FoundryCompletionProvider *self)
{
}

/**
 * foundry_completion_provider_complete:
 * @self: a [class@Foundry.CompletionProvider]
 * @request: a [class@Foundry.CompletionRequest]
 *
 * Asynchronously generate a list of completion items.
 *
 * Returns: (transfer full): [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.CompletionProposal] or
 *   rejects with error.
 */
DexFuture *
foundry_completion_provider_complete (FoundryCompletionProvider *self,
                                      FoundryCompletionRequest  *request)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (request));

  return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->complete (self, request);
}

/**
 * foundry_completion_provider_refilter:
 * @self: a [class@Foundry.CompletionProvider]
 * @request: a [class@Foundry.CompletionRequest]
 * @model: a [iface@Gio.ListModel]
 *
 * Asynchronously refilter @model or provides an alternate result set.
 *
 * Returns: (transfer full): [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.CompletionProposal] or
 *   rejects with error.
 */
DexFuture *
foundry_completion_provider_refilter (FoundryCompletionProvider *self,
                                      FoundryCompletionRequest  *request,
                                      GListModel                *model)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (request));
  dex_return_error_if_fail (G_IS_LIST_MODEL (model));

  return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->refilter (self, request, model);
}

DexFuture *
_foundry_completion_provider_load (FoundryCompletionProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));

  if (FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->load)
    return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->load (self);

  return dex_future_new_true ();
}

DexFuture *
_foundry_completion_provider_unload (FoundryCompletionProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));

  if (FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->unload)
    return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->unload (self);

  return dex_future_new_true ();
}
