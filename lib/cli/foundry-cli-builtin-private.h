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

void foundry_cli_builtin_build               (FoundryCliCommandTree *tree);
void foundry_cli_builtin_config_list         (FoundryCliCommandTree *tree);
void foundry_cli_builtin_config_switch       (FoundryCliCommandTree *tree);
void foundry_cli_builtin_deploy              (FoundryCliCommandTree *tree);
void foundry_cli_builtin_enter               (FoundryCliCommandTree *tree);
void foundry_cli_builtin_init                (FoundryCliCommandTree *tree);
void foundry_cli_builtin_device_list         (FoundryCliCommandTree *tree);
void foundry_cli_builtin_device_switch       (FoundryCliCommandTree *tree);
void foundry_cli_builtin_pipeline_flags      (FoundryCliCommandTree *tree);
void foundry_cli_builtin_pipeline_info       (FoundryCliCommandTree *tree);
void foundry_cli_builtin_pipeline_invalidate (FoundryCliCommandTree *tree);
void foundry_cli_builtin_pipeline_which      (FoundryCliCommandTree *tree);
void foundry_cli_builtin_run                 (FoundryCliCommandTree *tree);
void foundry_cli_builtin_sdk_install         (FoundryCliCommandTree *tree);
void foundry_cli_builtin_sdk_list            (FoundryCliCommandTree *tree);
void foundry_cli_builtin_sdk_shell           (FoundryCliCommandTree *tree);
void foundry_cli_builtin_sdk_switch          (FoundryCliCommandTree *tree);
void foundry_cli_builtin_sdk_which           (FoundryCliCommandTree *tree);
void foundry_cli_builtin_settings_get        (FoundryCliCommandTree *tree);
void foundry_cli_builtin_settings_set        (FoundryCliCommandTree *tree);
void foundry_cli_builtin_shell               (FoundryCliCommandTree *tree);
void foundry_cli_builtin_show                (FoundryCliCommandTree *tree);
void foundry_cli_builtin_vcs_list            (FoundryCliCommandTree *tree);
void foundry_cli_builtin_vcs_switch          (FoundryCliCommandTree *tree);

static inline void
_foundry_cli_builtin_register (FoundryCliCommandTree *tree)
{
  foundry_cli_builtin_build (tree);
  foundry_cli_builtin_config_list (tree);
  foundry_cli_builtin_config_switch (tree);
  foundry_cli_builtin_deploy (tree);
  foundry_cli_builtin_enter (tree);
  foundry_cli_builtin_init (tree);
  foundry_cli_builtin_device_list (tree);
  foundry_cli_builtin_device_switch (tree);
  foundry_cli_builtin_pipeline_flags (tree);
  foundry_cli_builtin_pipeline_info (tree);
  foundry_cli_builtin_pipeline_invalidate (tree);
  foundry_cli_builtin_pipeline_which (tree);
  foundry_cli_builtin_run (tree);
  foundry_cli_builtin_sdk_install (tree);
  foundry_cli_builtin_sdk_list (tree);
  foundry_cli_builtin_sdk_shell (tree);
  foundry_cli_builtin_sdk_switch (tree);
  foundry_cli_builtin_sdk_which (tree);
  foundry_cli_builtin_settings_get (tree);
  foundry_cli_builtin_settings_set (tree);
  foundry_cli_builtin_shell (tree);
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

static inline char **
foundry_cli_builtin_complete_list_model (FoundryCliOptions  *options,
                                         FoundryCommandLine *command_line,
                                         const char * const *argv,
                                         const char         *current,
                                         const char         *service_property,
                                         const char         *keyword_property)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (service_property != NULL);
  g_assert (keyword_property != NULL);

  if (!_foundry_cli_builtin_should_complete_id (argv, current))
    return NULL;

  builder = g_strv_builder_new ();

  if ((context = dex_await_object (foundry_cli_options_load_context (options, command_line), &error)))
    {
      g_autoptr(GListModel) model = NULL;
      guint n_items;

      g_object_get (context,
                    service_property, &model,
                    NULL);

      g_assert (G_IS_LIST_MODEL (model));

      n_items = g_list_model_get_n_items (model);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(GObject) object = g_list_model_get_item (model, i);
          g_autofree char *id = NULL;
          g_autofree char *spaced = NULL;

          g_object_get (object,
                        keyword_property, &id,
                        NULL);

          if (id == NULL)
            continue;

          spaced = g_strdup_printf ("%s ", id);

          if (current == NULL ||
              g_str_has_prefix (spaced, current))
            g_strv_builder_add (builder, spaced);
        }
    }

  return g_strv_builder_end (builder);
}

G_END_DECLS
