/* foundry-cli-command-tree-addin.c
 *
 * Copyright 2026 Christian Hergert
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

#include "foundry-cli-command-tree-addin-private.h"

/**
 * FoundryCliCommandTreeAddin:
 *
 * Addin interface for extending the CLI command tree.
 *
 * Since: 1.1
 */

G_DEFINE_ABSTRACT_TYPE (FoundryCliCommandTreeAddin, foundry_cli_command_tree_addin, G_TYPE_OBJECT)

static void
foundry_cli_command_tree_addin_class_init (FoundryCliCommandTreeAddinClass *klass)
{
}

static void
foundry_cli_command_tree_addin_init (FoundryCliCommandTreeAddin *self)
{
}

/**
 * _foundry_cli_command_tree_addin_load:
 * @self: a [class@Foundry.CliCommandTreeAddin]
 * @tree: a [class@Foundry.CliCommandTree]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value.
 */
DexFuture *
_foundry_cli_command_tree_addin_load (FoundryCliCommandTreeAddin *self,
                                      FoundryCliCommandTree      *tree)
{
  dex_return_error_if_fail (FOUNDRY_IS_CLI_COMMAND_TREE_ADDIN (self));
  dex_return_error_if_fail (FOUNDRY_IS_CLI_COMMAND_TREE (tree));

  if (FOUNDRY_CLI_COMMAND_TREE_ADDIN_GET_CLASS (self)->load)
    return FOUNDRY_CLI_COMMAND_TREE_ADDIN_GET_CLASS (self)->load (self, tree);

  return dex_future_new_true ();
}
