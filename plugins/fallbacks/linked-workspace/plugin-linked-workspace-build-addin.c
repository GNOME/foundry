/* plugin-linked-workspace-build-addin.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "plugin-linked-workspace-build-addin.h"

#include "foundry-context-private.h"
#include "gsettings-mapping.h"

#define LINKED_WORKSPACES "linked-workspaces"

struct _PluginLinkedWorkspaceBuildAddin
{
  FoundryBuildAddin  parent_instance;
  GPtrArray         *contexts;
  GPtrArray         *stages;
  FoundrySettings   *settings;
  DexFuture         *update;
  gulong             changed_handler;
  guint              stamp;
  guint              shutdown : 1;
};

G_DEFINE_FINAL_TYPE (PluginLinkedWorkspaceBuildAddin, plugin_linked_workspace_build_addin, FOUNDRY_TYPE_BUILD_ADDIN)

static void plugin_linked_workspace_build_addin_update (PluginLinkedWorkspaceBuildAddin *self);

static void
plugin_linked_workspace_build_addin_remove_all (PluginLinkedWorkspaceBuildAddin *self)
{
  g_assert (PLUGIN_IS_LINKED_WORKSPACE_BUILD_ADDIN (self));

  if (self->stages != NULL)
    {
      g_autoptr(FoundryBuildPipeline) pipeline = NULL;

      if ((pipeline = foundry_build_addin_dup_pipeline (FOUNDRY_BUILD_ADDIN (self))))
        {
          for (guint i = 0; i < self->stages->len; i++)
            foundry_build_pipeline_remove_stage (pipeline, g_ptr_array_index (self->stages, i));
        }

      if (self->stages->len > 0)
        g_ptr_array_remove_range (self->stages, 0, self->stages->len);
    }

  if (self->contexts != NULL && self->contexts->len > 0)
    g_ptr_array_remove_range (self->contexts, 0, self->contexts->len);
}

static void
plugin_linked_workspace_build_addin_invalidate_pipeline (PluginLinkedWorkspaceBuildAddin *self,
                                                         FoundryBuildManager             *build_manager)
{
  g_assert (PLUGIN_IS_LINKED_WORKSPACE_BUILD_ADDIN (self));
  g_assert (FOUNDRY_IS_BUILD_MANAGER (build_manager));

  g_signal_handlers_disconnect_by_func (build_manager,
                                        G_CALLBACK (plugin_linked_workspace_build_addin_invalidate_pipeline),
                                        self);

  plugin_linked_workspace_build_addin_update (self);
}

static FoundryContext *
find_or_load_context (const char  *state_directory,
                      const char  *project_directory,
                      gboolean    *borrowed,
                      GError     **error)
{
  g_autoptr(FoundryContext) context = NULL;

  *borrowed = FALSE;

  if ((context = _foundry_context_find (state_directory)))
    {
      *borrowed = TRUE;
      return g_steal_pointer (&context);
    }

  return dex_await_object (foundry_context_new (state_directory,
                                                project_directory,
                                                FOUNDRY_CONTEXT_FLAGS_NONE,
                                                NULL),
                           error);
}

static DexFuture *
plugin_linked_workspace_build_addin_update_fiber (PluginLinkedWorkspaceBuildAddin *self,
                                                  guint                            stamp)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autoptr(GVariant) variant = NULL;
  guint n_children;

  g_assert (PLUGIN_IS_LINKED_WORKSPACE_BUILD_ADDIN (self));

  if (self->stamp != stamp)
    return dex_future_new_true ();

  if (self->settings == NULL ||
      !(variant = foundry_settings_get_value (self->settings, LINKED_WORKSPACES)) ||
      !g_variant_is_of_type (variant, G_VARIANT_TYPE ("aa{sv}")) ||
      (n_children = g_variant_n_children (variant)) == 0)
    return dex_future_new_true ();

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    return dex_future_new_true ();

  pipeline = foundry_build_addin_dup_pipeline (FOUNDRY_BUILD_ADDIN (self));

  for (guint i = 0; i < n_children; i++)
    {
      g_autoptr(GVariant) info = g_variant_get_child_value (variant, i);
      g_autofree char *project_directory = NULL;
      g_autofree char *state_directory = NULL;
      g_autoptr(FoundryBuildManager) build_manager = NULL;
      g_autoptr(FoundryBuildPipeline) other_pipeline = NULL;
      g_autoptr(FoundryBuildStage) stage = NULL;
      g_autoptr(FoundryContext) other_context = NULL;
      g_autoptr(GVariantDict) dict = NULL;
      g_autoptr(GVariant) phasev = NULL;
      g_autoptr(GVariant) linked_phasev = NULL;
      g_autoptr(GFile) project_directory_file = NULL;
      g_autoptr(GFile) state_directory_file = NULL;
      g_autoptr(GError) error = NULL;
      g_auto(GValue) phase_value = G_VALUE_INIT;
      g_auto(GValue) linked_phase_value = G_VALUE_INIT;
      gboolean borrowed = FALSE;
      FoundryBuildPipelinePhase phase;
      FoundryBuildPipelinePhase linked_phase;

      dict = g_variant_dict_new (info);

      if (!g_variant_dict_lookup (dict, "project-directory", "s", &project_directory))
        continue;

      if (!g_variant_dict_lookup (dict, "state-directory", "s", &state_directory))
        continue;

      project_directory_file = g_file_new_for_uri (project_directory);
      state_directory_file = g_file_new_for_uri (state_directory);

      if (!g_file_is_native (project_directory_file) ||
          !g_file_is_native (state_directory_file))
        continue;

      g_value_init (&phase_value, FOUNDRY_TYPE_BUILD_PIPELINE_PHASE);
      if (!(phasev = g_variant_dict_lookup_value (dict, "phase", G_VARIANT_TYPE ("as"))) ||
          !g_settings_get_mapping (&phase_value, phasev, NULL))
        continue;
      phase = g_value_get_flags (&phase_value);

      g_value_init (&linked_phase_value, FOUNDRY_TYPE_BUILD_PIPELINE_PHASE);
      if (!(linked_phasev = g_variant_dict_lookup_value (dict, "linked-phase", G_VARIANT_TYPE ("as"))) ||
          !g_settings_get_mapping (&linked_phase_value, linked_phasev, NULL))
        continue;
      linked_phase = g_value_get_flags (&linked_phase_value);

      if (!(other_context = find_or_load_context (g_file_peek_path (state_directory_file),
                                                  g_file_peek_path (project_directory_file),
                                                  &borrowed, &error)))
        {
          g_warning ("Failed to load linked workspace: %s", error->message);
          continue;
        }

      build_manager = foundry_context_dup_build_manager (other_context);

      g_signal_connect_object (build_manager,
                               "pipeline-invalidated",
                               G_CALLBACK (plugin_linked_workspace_build_addin_invalidate_pipeline),
                               self,
                               G_CONNECT_SWAPPED);

      if (!(other_pipeline = dex_await_object (foundry_build_manager_load_pipeline (build_manager), &error)))
        {
          g_warning ("Failed to load linked workspace pipeline: %s", error->message);
          continue;
        }

      if ((stage = foundry_linked_pipeline_stage_new_full (context, other_pipeline, phase, linked_phase)) &&
          self->stamp == stamp)
        {
          if (!borrowed)
            g_ptr_array_add (self->contexts, g_object_ref (other_context));

          g_ptr_array_add (self->stages, g_object_ref (stage));
          foundry_build_pipeline_add_stage (pipeline, stage);
        }
    }

  return dex_future_new_true ();
}

static void
plugin_linked_workspace_build_addin_update (PluginLinkedWorkspaceBuildAddin *self)
{
  g_assert (PLUGIN_IS_LINKED_WORKSPACE_BUILD_ADDIN (self));

  if (self->shutdown)
    return;

  self->stamp++;

  dex_clear (&self->update);

  plugin_linked_workspace_build_addin_remove_all (self);

  self->update = foundry_scheduler_spawn (NULL, 0,
                                          G_CALLBACK (plugin_linked_workspace_build_addin_update_fiber),
                                          2,
                                          PLUGIN_TYPE_LINKED_WORKSPACE_BUILD_ADDIN, self,
                                          G_TYPE_UINT, self->stamp);
}

static DexFuture *
do_nothing (DexFuture *future,
            gpointer   user_data)
{
  return NULL;
}

static void
shutdown_and_release (FoundryContext *context)
{
  return dex_future_disown (dex_future_finally (foundry_context_shutdown (context),
                                                do_nothing, context, g_object_unref));
}

static DexFuture *
plugin_linked_workspace_build_addin_load (FoundryBuildAddin *addin)
{
  PluginLinkedWorkspaceBuildAddin *self = PLUGIN_LINKED_WORKSPACE_BUILD_ADDIN (addin);
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GError) error = NULL;

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  self->contexts = g_ptr_array_new_with_free_func ((GDestroyNotify) shutdown_and_release);
  self->stages = g_ptr_array_new_with_free_func (g_object_unref);
  self->settings = foundry_context_load_settings (context, "app.devsuite.foundry.build", NULL);
  self->changed_handler =
    g_signal_connect_object (self->settings,
                             "changed::" LINKED_WORKSPACES,
                             G_CALLBACK (plugin_linked_workspace_build_addin_update),
                             self,
                             G_CONNECT_SWAPPED);

  plugin_linked_workspace_build_addin_update (self);

  return dex_ref (self->update);
}

static DexFuture *
plugin_linked_workspace_build_addin_unload (FoundryBuildAddin *addin)
{
  PluginLinkedWorkspaceBuildAddin *self = PLUGIN_LINKED_WORKSPACE_BUILD_ADDIN (addin);

  self->shutdown = TRUE;

  g_clear_signal_handler (&self->changed_handler, self->settings);
  g_clear_object (&self->settings);
  dex_clear (&self->update);

  plugin_linked_workspace_build_addin_remove_all (self);

  g_clear_pointer (&self->stages, g_ptr_array_unref);
  g_clear_pointer (&self->contexts, g_ptr_array_unref);

  self->stamp++;

  return dex_future_new_true ();
}

static void
plugin_linked_workspace_build_addin_class_init (PluginLinkedWorkspaceBuildAddinClass *klass)
{
  FoundryBuildAddinClass *build_addin_class = FOUNDRY_BUILD_ADDIN_CLASS (klass);

  build_addin_class->load = plugin_linked_workspace_build_addin_load;
  build_addin_class->unload = plugin_linked_workspace_build_addin_unload;
}

static void
plugin_linked_workspace_build_addin_init (PluginLinkedWorkspaceBuildAddin *self)
{
}
