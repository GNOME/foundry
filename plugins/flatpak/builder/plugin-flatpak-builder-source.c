/* plugin-flatpak-builder-source.c
 *
 * Copyright 2015 Red Hat, Inc
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

#include "plugin-flatpak-builder-source.h"

#if 0
# include "plugin-flatpak-builder-source-archive.h"
# include "plugin-flatpak-builder-source-patch.h"
# include "plugin-flatpak-builder-source-git.h"
# include "plugin-flatpak-builder-source-bzr.h"
# include "plugin-flatpak-builder-source-svn.h"
# include "plugin-flatpak-builder-source-file.h"
# include "plugin-flatpak-builder-source-dir.h"
# include "plugin-flatpak-builder-source-script.h"
# include "plugin-flatpak-builder-source-inline.h"
# include "plugin-flatpak-builder-source-shell.h"
# include "plugin-flatpak-builder-source-extra-data.h"
#endif

static void serializable_iface_init (JsonSerializableIface *serializable_iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (PluginFlatpakBuilderSource, plugin_flatpak_builder_source, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (JSON_TYPE_SERIALIZABLE, serializable_iface_init));

enum {
  PROP_0,
  PROP_DEST,
  PROP_ONLY_ARCHES,
  PROP_SKIP_ARCHES,
  N_PROPS
};


static void
plugin_flatpak_builder_source_finalize (GObject *object)
{
  PluginFlatpakBuilderSource *self = PLUGIN_FLATPAK_BUILDER_SOURCE (object);

  g_clear_object (&self->base_dir);
  g_clear_pointer (&self->dest, g_free);
  g_clear_pointer (&self->only_arches, g_strfreev);
  g_clear_pointer (&self->skip_arches, g_strfreev);

  G_OBJECT_CLASS (plugin_flatpak_builder_source_parent_class)->finalize (object);
}

static void
plugin_flatpak_builder_source_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  PluginFlatpakBuilderSource *self = PLUGIN_FLATPAK_BUILDER_SOURCE (object);

  switch (prop_id)
    {
    case PROP_DEST:
      g_value_set_string (value, self->dest);
      break;

    case PROP_ONLY_ARCHES:
      g_value_set_boxed (value, self->only_arches);
      break;

    case PROP_SKIP_ARCHES:
      g_value_set_boxed (value, self->skip_arches);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_builder_source_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PluginFlatpakBuilderSource *self = PLUGIN_FLATPAK_BUILDER_SOURCE (object);
  gchar **tmp;

  switch (prop_id)
    {
    case PROP_DEST:
      g_free (self->dest);
      self->dest = g_value_dup_string (value);
      break;

    case PROP_ONLY_ARCHES:
      tmp = self->only_arches;
      self->only_arches = g_strdupv (g_value_get_boxed (value));
      g_strfreev (tmp);
      break;

    case PROP_SKIP_ARCHES:
      tmp = self->skip_arches;
      self->skip_arches = g_strdupv (g_value_get_boxed (value));
      g_strfreev (tmp);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}
static gboolean
plugin_flatpak_builder_source_real_show_deps (PluginFlatpakBuilderSource  *self,
                                              GError        **error)
{
  return TRUE;
}

static gboolean
plugin_flatpak_builder_source_real_download (PluginFlatpakBuilderSource  *self,
                                             gboolean        update_vcs,
                                             BuilderContext *context,
                                             GError        **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "Download not implemented for type %s", g_type_name_from_instance ((GTypeInstance *) self));
  return FALSE;
}

static gboolean
plugin_flatpak_builder_source_real_extract (PluginFlatpakBuilderSource  *self,
                                            GFile          *dest,
                                            GFile          *source_dir,
                                            BuilderOptions *build_options,
                                            BuilderContext *context,
                                            GError        **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "Extract not implemented for type %s", g_type_name_from_instance ((GTypeInstance *) self));
  return FALSE;
}

static gboolean
plugin_flatpak_builder_source_real_bundle (PluginFlatpakBuilderSource  *self,
                                           BuilderContext *context,
                                           GError        **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "Bundle not implemented for type %s", g_type_name_from_instance ((GTypeInstance *) self));
  return FALSE;
}

static gboolean
plugin_flatpak_builder_source_real_update (PluginFlatpakBuilderSource  *self,
                                           BuilderContext *context,
                                           GError        **error)
{
  return TRUE;
}

static void
plugin_flatpak_builder_source_class_init (PluginFlatpakBuilderSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_flatpak_builder_source_finalize;
  object_class->get_property = plugin_flatpak_builder_source_get_property;
  object_class->set_property = plugin_flatpak_builder_source_set_property;

  klass->show_deps = plugin_flatpak_builder_source_real_show_deps;
  klass->download = plugin_flatpak_builder_source_real_download;
  klass->extract = plugin_flatpak_builder_source_real_extract;
  klass->bundle = plugin_flatpak_builder_source_real_bundle;
  klass->update = plugin_flatpak_builder_source_real_update;

  g_object_class_install_property (object_class,
                                   PROP_DEST,
                                   g_param_spec_string ("dest",
                                                        "",
                                                        "",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_ONLY_ARCHES,
                                   g_param_spec_boxed ("only-arches",
                                                       "",
                                                       "",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_SKIP_ARCHES,
                                   g_param_spec_boxed ("skip-arches",
                                                       "",
                                                       "",
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE));
}

static void
plugin_flatpak_builder_source_init (PluginFlatpakBuilderSource *self)
{
}

static GParamSpec *
plugin_flatpak_builder_source_find_property (JsonSerializable *serializable,
                              const char       *name)
{
  if (strcmp (name, "type") == 0)
    return NULL;
  return builder_serializable_find_property (serializable, name);
}

static void
serializable_iface_init (JsonSerializableIface *serializable_iface)
{
  serializable_iface->serialize_property = builder_serializable_serialize_property;
  serializable_iface->deserialize_property = builder_serializable_deserialize_property;
  serializable_iface->find_property = plugin_flatpak_builder_source_find_property;
  serializable_iface->list_properties = builder_serializable_list_properties;
  serializable_iface->set_property = builder_serializable_set_property;
  serializable_iface->get_property = builder_serializable_get_property;
}

JsonNode *
plugin_flatpak_builder_source_to_json (PluginFlatpakBuilderSource *self)
{
  const char *type = NULL;
  JsonObject *object;
  JsonNode *node;

  g_return_val_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self), NULL);

  node = json_gobject_serialize (G_OBJECT (self));
  object = json_node_get_object (node);

  if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_ARCHIVE (self))
    type = "archive";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_FILE (self))
    type = "file";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_DIR (self))
    type = "dir";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_SCRIPT (self))
    type = "script";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_INLINE (self))
    type = "inline";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_SHELL (self))
    type = "shell";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_EXTRA_DATA (self))
    type = "extra-data";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_PATCH (self))
    type = "patch";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_GIT (self))
    type = "git";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_BZR (self))
    type = "bzr";
  else if (PLUGIN_IS_FLATPAK_BUILDER_SOURCE_SVN (self))
    type = "svn";

  if (type != NULL)
    json_object_set_string_member (object, "type", type);

  return node;
}

DexFuture *
plugin_flatpak_builder_source_from_json (JsonNode *node)
{
  g_autoptr(PluginFlatpakBuilderSource) source = NULL;
  JsonObject *object;
  const gchar *type;

  dex_return_error_if_fail (node != NULL);
  dex_return_error_if_fail (JSON_NODE_HOLDS_OBJECT (node));

  object = json_node_get_object (node);
  type = json_object_get_string_member (object, "type");

  if (type == NULL)
    source = NULL;
  else if (strcmp (type, "archive") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_ARCHIVE, node);
  else if (strcmp (type, "file") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_FILE, node);
  else if (strcmp (type, "dir") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_DIR, node);
  else if (strcmp (type, "script") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_SCRIPT, node);
  else if (strcmp (type, "inline") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_INLINE, node);
  else if (strcmp (type, "shell") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_SHELL, node);
  else if (strcmp (type, "extra-data") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_EXTRA_DATA, node);
  else if (strcmp (type, "patch") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_PATCH, node);
  else if (strcmp (type, "git") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_GIT, node);
  else if (strcmp (type, "bzr") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_BZR, node);
  else if (strcmp (type, "svn") == 0)
    source = (PluginFlatpakBuilderSource *) json_gobject_deserialize (PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE_SVN, node);
  else
    source = NULL;

  if (source == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Failed to deserialize manifest");

  if (!dex_await (plugin_flatpak_builder_source_validate (source), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&source));
}

DexFuture *
plugin_flatpak_builder_source_download (PluginFlatpakBuilderSource  *self,
                                        gboolean                     update_vcs,
                                        PluginFlatpakBuilderContext *context)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self));
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (context));

  return PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->download (self, update_vcs, context);
}

/* Ensure the destination exists (by making directories if needed) and
   that it is inside the build directory. It could be outside the build
   dir either if the specified path makes it so, of if some symlink inside
   the source tree points outside it. */
static gboolean
ensure_dir_inside_toplevel (GFile   *dest,
                            GFile   *toplevel_dir,
                            GError **error)
{
  if (!g_file_query_exists (dest, NULL))
    {
      g_autoptr(GFile) parent = g_file_get_parent (dest);
      g_autoptr(GError) error = NULL;

      if (parent == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_FOUND,
                       "No parent directory found");
          return FALSE;
        }

      if (!ensure_dir_inside_toplevel (parent, toplevel_dir, error))
        return FALSE;

      if (!dex_await (dex_file_make_directory (dest), error))
        return FALSE;
    }

  if (!flatpak_file_is_in (dest, toplevel_dir))
    {
      g_set_error (G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "dest is not pointing inside build directory");
      return FALSE;
    }

  return TRUE;
}

DexFuture *
plugin_flatpak_builder_source_extract (PluginFlatpakBuilderSource  *self,
                                       GFile                       *source_dir,
                                       PluginFlatpakBuilderOptions *build_options,
                                       PluginFlatpakBuilderContext *context)
{
  g_autoptr(GFile) real_dest = NULL;

  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self));
  dex_return_error_if_fail (G_IS_FILE (source_file));
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_OPTIONS (build_options));
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (context));

  if (self->dest != NULL)
    {
      g_autoptr(GError) error = NULL;

      real_dest = g_file_resolve_relative_path (source_dir, self->dest);

      if (!ensure_dir_inside_toplevel (real_dest, source_dir, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }
  else
    {
      real_dest = g_object_ref (source_dir);
    }

  return PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->extract (self, source_dir, build_options, context);
}

DexFuture *
plugin_flatpak_builder_source_bundle (PluginFlatpakBuilderSource  *self,
                                      PluginFlatpakBuilderContext *context)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self));
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (context));

  if (PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->bundle)
    return PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->bundle (self, context);

  return dex_future_new_true ();
}

DexFuture *
plugin_flatpak_builder_source_update (PluginFlatpakBuilderSource  *self,
                                      PluginFlatpakBuilderContext *context)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self));
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (context));

  if (PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->update)
    return PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->update (self, context);

  return dex_future_new_true ();
}

DexFuture *
plugin_flatpak_builder_source_checksum (PluginFlatpakBuilderSource  *self,
                                        PluginFlatpakBuilderCache   *cache,
                                        PluginFlatpakBuilderContext *context)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self));
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CACHE (cache));
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (context));

  plugin_flatpak_builder_cache_checksum_str (cache, self->dest);
  plugin_flatpak_builder_cache_checksum_strv (cache, self->only_arches);
  plugin_flatpak_builder_cache_checksum_strv (cache, self->skip_arches);

  if (PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->cache)
    return PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->cache (self, cache, context);

  return dex_future_new_true ();
}

DexFuture *
plugin_flatpak_builder_source_finish (PluginFlatpakBuilderSource  *self,
                                      const char * const          *args,
                                      PluginFlatpakBuilderContext *context)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self));
  dex_return_error_if_fail (args != NULL);
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (self));

  if (PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->finish)
    return PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->finish (self, args, context);

  return dex_future_new_true ();
}

DexFuture *
plugin_flatpak_builder_source_validate (PluginFlatpakBuilderSource *self)
{
  dex_return_error_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self));

  if (PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->validate)
    return PLUGIN_FLATPAK_BUILDER_SOURCE_GET_CLASS (self)->validate (self);

  return dex_future_new_true ();
}

gboolean
plugin_flatpak_builder_source_is_enabled (PluginFlatpakBuilderSource  *self,
                                          PluginFlatpakBuilderContext *context)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self));
  g_return_if_fail (PLUGIN_IS_FLATPAK_BUILDER_CONTEXT (context));

  if (self->only_arches != NULL &&
      self->only_arches[0] != NULL &&
      !g_strv_contains ((const char * const *) self->only_arches, builder_context_get_arch (context)))
    return FALSE;

  if (self->skip_arches != NULL &&
      g_strv_contains ((const char * const *)self->skip_arches, builder_context_get_arch (context)))
    return FALSE;

  return TRUE;
}

void
plugin_flatpak_builder_source_set_base_dir (PluginFlatpakBuilderSource *self,
                                            GFile                      *base_dir)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_BUILDER_SOURCE (self));
  g_return_if_fail (G_IS_FILE (base_dir));

  if (g_set_object (&self->base_dir, base_dir))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BASE_DIR]);
}

