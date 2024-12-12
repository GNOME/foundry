/* foundry-cli-builtin-private.h
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

#pragma once

#include "foundry-cli-command.h"
#include "foundry-cli-command-tree.h"
#include "foundry-command-line.h"
#include "foundry-types.h"
#include "foundry-util-private.h"

G_BEGIN_DECLS

void foundry_cli_builtin_build         (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_config_list   (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_config_switch (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_enter         (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_init          (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_device_list   (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_device_switch (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_sdk_list      (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_sdk_switch    (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_settings_get  (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_settings_set  (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_show          (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_vcs_list      (FoundryCliCommandTree   *tree);
void foundry_cli_builtin_vcs_switch    (FoundryCliCommandTree   *tree);

static inline void
_foundry_cli_builtin_register (FoundryCliCommandTree *tree)
{
  foundry_cli_builtin_build (tree);
  foundry_cli_builtin_config_list (tree);
  foundry_cli_builtin_config_switch (tree);
  foundry_cli_builtin_enter (tree);
  foundry_cli_builtin_init (tree);
  foundry_cli_builtin_device_list (tree);
  foundry_cli_builtin_device_switch (tree);
  foundry_cli_builtin_sdk_list (tree);
  foundry_cli_builtin_sdk_switch (tree);
  foundry_cli_builtin_settings_get (tree);
  foundry_cli_builtin_settings_set (tree);
  foundry_cli_builtin_show (tree);
  foundry_cli_builtin_vcs_list (tree);
  foundry_cli_builtin_vcs_switch (tree);
}

static inline gboolean
_foundry_cli_builtin_should_complete_id (const char * const *argv,
                                         const char         *current)
{
  if (g_strv_length ((char **)argv) > 2 ||
      (g_strv_length ((char **)argv) == 2 && foundry_str_empty0 (current)))
    return FALSE;

  return TRUE;
}

G_END_DECLS
