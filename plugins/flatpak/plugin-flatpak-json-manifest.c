/* plugin-flatpak-json-manifest.c
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

#include "plugin-flatpak-json-manifest.h"
#include "plugin-flatpak-manifest-private.h"

#define MAX_MANIFEST_SIZE_IN_BYTES (1024L*256L) /* 256kb */

struct _PluginFlatpakJsonManifest
{
  PluginFlatpakManifest  parent_instance;
  JsonNode              *json;
  JsonObject            *primary_module;
};

struct _PluginFlatpakJsonManifestClass
{
  PluginFlatpakManifestClass parent_class;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakJsonManifest, plugin_flatpak_json_manifest, PLUGIN_TYPE_FLATPAK_MANIFEST)

static JsonObject *
discover_primary_module (PluginFlatpakJsonManifest  *self,
                         JsonObject                 *parent,
                         const char                 *dir_name,
                         gboolean                    is_root,
                         GError                    **error)
{
  JsonArray *ar;
  JsonNode *modules;
  guint n_elements;

  g_assert (PLUGIN_IS_FLATPAK_JSON_MANIFEST (self));
  g_assert (parent != NULL);
  g_assert (dir_name != NULL);

  if (!json_object_has_member (parent, "modules") ||
      !(modules = json_object_get_member (parent, "modules")) ||
      !JSON_NODE_HOLDS_ARRAY (modules) ||
      !(ar = json_node_get_array (modules)))
    goto no_match;

  n_elements = json_array_get_length (ar);

  for (guint i = n_elements; i > 0; i--)
    {
      JsonNode *element = json_array_get_element (ar, i - 1);
      const gchar *name;
      JsonObject *obj;

      if (!JSON_NODE_HOLDS_OBJECT (element) ||
          !(obj = json_node_get_object (element)) ||
          !(name = json_object_get_string_member (obj, "name")))
        continue;

      if (foundry_str_equal0 (name, dir_name))
        {
          g_set_str (&PLUGIN_FLATPAK_MANIFEST (self)->primary_module_name, name);
          return obj;
        }

      if (json_object_has_member (obj, "modules"))
        {
          JsonObject *subobj;

          if ((subobj = discover_primary_module (self, obj, dir_name, FALSE, NULL)))
            return subobj;
        }
    }

  if (is_root)
    {
      for (guint i = n_elements; i > 0; i--)
        {
          JsonNode *element = json_array_get_element (ar, i - 1);
          const gchar *name;
          JsonObject *obj;

          if (!JSON_NODE_HOLDS_OBJECT (element) ||
              !(obj = json_node_get_object (element)) ||
              !(name = json_object_get_string_member (obj, "name")))
            continue;

          g_set_str (&PLUGIN_FLATPAK_MANIFEST (self)->primary_module_name, name);
          return obj;
        }
    }

no_match:
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to locate primary module in modules");

  return NULL;
}

static char **
plugin_flatpak_json_manifest_get_commands (PluginFlatpakManifest *manifest)
{
  PluginFlatpakJsonManifest *self = (PluginFlatpakJsonManifest *)manifest;
  g_autoptr(GStrvBuilder) commands_builder = NULL;
  const char *build_system;
  JsonArray *ar;
  gsize length;

  g_assert (PLUGIN_IS_FLATPAK_JSON_MANIFEST (self));

  if (self->primary_module == NULL)
    return NULL;

  if (!(build_system = json_object_get_string_member (self->primary_module, "buildsystem")) ||
      !foundry_str_equal0 (build_system, "simple"))
    return NULL;

  if (!json_object_has_member (self->primary_module, "build-commands"))
    return NULL;

  if (!(ar = json_object_get_array_member (self->primary_module, "build-commands")))
    return NULL;

  length = json_array_get_length (ar);
  commands_builder = g_strv_builder_new ();

  for (gsize i = 0; i < length; i++)
    {
      const char *str = json_array_get_string_element (ar, i);

      g_strv_builder_add (commands_builder, str);
    }

  return g_strv_builder_end (commands_builder);
}

static void
plugin_flatpak_json_manifest_finalize (GObject *object)
{
  PluginFlatpakJsonManifest *self = (PluginFlatpakJsonManifest *)object;

  g_clear_pointer (&self->primary_module, json_object_unref);
  g_clear_pointer (&self->json, json_node_unref);

  G_OBJECT_CLASS (plugin_flatpak_json_manifest_parent_class)->finalize (object);
}

static void
plugin_flatpak_json_manifest_class_init (PluginFlatpakJsonManifestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PluginFlatpakManifestClass *manifest_class = PLUGIN_FLATPAK_MANIFEST_CLASS (klass);

  object_class->finalize = plugin_flatpak_json_manifest_finalize;

  manifest_class->get_commands = plugin_flatpak_json_manifest_get_commands;
}

static void
plugin_flatpak_json_manifest_init (PluginFlatpakJsonManifest *self)
{
}

static gboolean
plugin_flatpak_json_manifest_validate (JsonNode *root)
{
  const char *id;
  const char *command;
  const char *runtime;
  const char *runtime_version;

  g_assert (root != NULL);

  if (!(id = foundry_json_node_get_string_at (root, "id", NULL)))
    id = foundry_json_node_get_string_at (root, "app-id", NULL);

  runtime = foundry_json_node_get_string_at (root, "runtime", NULL);
  runtime_version = foundry_json_node_get_string_at (root, "runtime-version", NULL);
  command = foundry_json_node_get_string_at (root, "command", NULL);

  if (id == NULL ||
      runtime == NULL ||
      runtime_version == NULL ||
      command == NULL)
    return FALSE;

  return TRUE;
}

static DexFuture *
plugin_flatpak_json_manifest_load_fiber (gpointer user_data)
{
  PluginFlatpakJsonManifest *self = user_data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GFileInfo) info = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *dir_name = NULL;
  const char *id = NULL;
  const char *build_system;
  JsonObject *primary_module;
  JsonObject *root_obj;
  JsonNode *root;

  g_assert (PLUGIN_IS_FLATPAK_JSON_MANIFEST (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  file = plugin_flatpak_manifest_dup_file (PLUGIN_FLATPAK_MANIFEST (self));

  /* First check that the file size isn't absurd */
  if (!(info = dex_await_object (dex_file_query_info (file,
                                                      G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                                      G_FILE_QUERY_INFO_NONE,
                                                      G_PRIORITY_DEFAULT),
                                 &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (g_file_info_get_size (info) > MAX_MANIFEST_SIZE_IN_BYTES)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Manifest too large");

  /* Then parse the JSON document */
  parser = json_parser_new ();
  if (!dex_await (foundry_json_parser_load_from_file (parser, file), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Validate some basic information about the manifest */
  root = json_parser_get_root (parser);
  if (!plugin_flatpak_json_manifest_validate (root))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "File does not appear to be a manifest");

  /* Try to discover the primary module */
  workdir = foundry_context_dup_project_directory (context);
  dir_name = g_file_get_basename (workdir);
  root_obj = json_node_get_object (root);
  if (!(primary_module = discover_primary_module (self, root_obj, dir_name, TRUE, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Try to locate the build system */
  if (!(build_system = json_object_get_string_member (primary_module, "buildsystem")))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Manifest is missing buildsystem in primary module");

  if (!(id = foundry_json_node_get_string_at (root, "id", NULL)))
    id = foundry_json_node_get_string_at (root, "app-id", NULL);

  /* Save information around for use */
  _plugin_flatpak_manifest_set_id (PLUGIN_FLATPAK_MANIFEST (self), id);
  _plugin_flatpak_manifest_set_runtime (PLUGIN_FLATPAK_MANIFEST (self),
                                        foundry_json_node_get_string_at (root, "runtime", NULL));
  _plugin_flatpak_manifest_set_runtime_version (PLUGIN_FLATPAK_MANIFEST (self),
                                                foundry_json_node_get_string_at (root, "runtime-version", NULL));
  _plugin_flatpak_manifest_set_command (PLUGIN_FLATPAK_MANIFEST (self),
                                        foundry_json_node_get_string_at (root, "command", NULL));
  _plugin_flatpak_manifest_set_build_system (PLUGIN_FLATPAK_MANIFEST (self), build_system);

  /* Save the JSON for use later */
  self->primary_module = json_object_ref (primary_module);
  self->json = json_parser_steal_root (parser);

  /* Allow the base class to resolve things */
  dex_await (_plugin_flatpak_manifest_resolve (PLUGIN_FLATPAK_MANIFEST (self)), NULL);

  return dex_future_new_take_object (g_object_ref (self));
}

DexFuture *
plugin_flatpak_json_manifest_new (FoundryContext *context,
                                  GFile          *file)
{
  PluginFlatpakJsonManifest *self;

  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));
  dex_return_error_if_fail (G_IS_FILE (file));

  self = g_object_new (PLUGIN_TYPE_FLATPAK_JSON_MANIFEST,
                       "context", context,
                       "file", file,
                       NULL);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_flatpak_json_manifest_load_fiber,
                              g_steal_pointer (&self),
                              g_object_unref);
}
