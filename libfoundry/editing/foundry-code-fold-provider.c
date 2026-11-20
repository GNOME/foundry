/* foundry-code-fold-provider.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-code-fold-provider.h"
#include "foundry-text-document.h"

/**
 * FoundryCodeFoldProvider:
 *
 * Produces complete code fold snapshots for a text document.
 *
 * Since: 1.3
 */

G_DEFINE_ABSTRACT_TYPE (FoundryCodeFoldProvider, foundry_code_fold_provider, FOUNDRY_TYPE_CONTEXTUAL)

static void
foundry_code_fold_provider_class_init (FoundryCodeFoldProviderClass *klass)
{
}

static void
foundry_code_fold_provider_init (FoundryCodeFoldProvider *self)
{
}

/**
 * foundry_code_fold_provider_list_folds:
 * @self: a fold provider
 * @document: the document to fold
 * @contents: captured UTF-8 contents of the document
 *
 * Returns: (transfer full): a future resolving to a #GListModel of #FoundryCodeFold
 * Since: 1.3
 */
DexFuture *
foundry_code_fold_provider_list_folds (FoundryCodeFoldProvider *self,
                                       FoundryTextDocument     *document,
                                       GBytes                  *contents)
{
  dex_return_error_if_fail (FOUNDRY_IS_CODE_FOLD_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_DOCUMENT (document));
  dex_return_error_if_fail (contents != NULL);

  if (FOUNDRY_CODE_FOLD_PROVIDER_GET_CLASS (self)->list_folds != NULL)
    return FOUNDRY_CODE_FOLD_PROVIDER_GET_CLASS (self)->list_folds (self, document, contents);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Code folding is not supported");
}
