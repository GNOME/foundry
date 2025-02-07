/* plugin-flatpak-arch-options.c
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

#include "plugin-flatpak-arch-options.h"
#include "plugin-flatpak-serializable-private.h"

struct _PluginFlatpakArchOptions
{
  PluginFlatpakSerializable parent_instance;
  GHashTable *arches;
};

G_DEFINE_FINAL_TYPE (PluginFlatpakArchOptions, plugin_flatpak_arch_options, PLUGIN_TYPE_FLATPAK_SERIALIZABLE)

static DexFuture *
plugin_flatpak_arch_options_deserialize (PluginFlatpakSerializable *serializable,
                                         JsonNode                  *node)
{
  PluginFlatpakArchOptions *self = (PluginFlatpakArchOptions *)serializable;
  g_autoptr(GFile) base_dir = NULL;
  JsonObject *object;
  JsonObjectIter iter;
  const char *member_name;
  JsonNode *member_node;

  g_assert (PLUGIN_IS_FLATPAK_ARCH_OPTIONS (self));
  g_assert (node != NULL);

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Expected object for arch");

  base_dir = _plugin_flatpak_serializable_dup_base_dir (serializable);
  object = json_node_get_object (node);

  json_object_iter_init (&iter, object);
  while (json_object_iter_next (&iter, &member_name, &member_node))
    {
      g_autoptr(PluginFlatpakSerializable) options = NULL;
      g_autoptr(GError) error = NULL;

      options = _plugin_flatpak_serializable_new (PLUGIN_TYPE_FLATPAK_OPTIONS, base_dir);

      if (!dex_await (_plugin_flatpak_serializable_deserialize (options, member_node), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_hash_table_replace (self->arches, g_strdup (member_name), g_steal_pointer (&options));
    }

  return dex_future_new_true ();
}

static void
plugin_flatpak_arch_options_finalize (GObject *object)
{
  PluginFlatpakArchOptions *self = (PluginFlatpakArchOptions *)object;

  g_clear_pointer (&self->arches, g_hash_table_unref);

  G_OBJECT_CLASS (plugin_flatpak_arch_options_parent_class)->finalize (object);
}

static void
plugin_flatpak_arch_options_class_init (PluginFlatpakArchOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PluginFlatpakSerializableClass *serializable_class = PLUGIN_FLATPAK_SERIALIZABLE_CLASS (klass);

  object_class->finalize = plugin_flatpak_arch_options_finalize;

  serializable_class->deserialize = plugin_flatpak_arch_options_deserialize;
}

static void
plugin_flatpak_arch_options_init (PluginFlatpakArchOptions *self)
{
  self->arches = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

char **
plugin_flatpak_arch_options_dup_arches (PluginFlatpakArchOptions *self)
{
  g_auto(GStrv) keys = NULL;

  g_return_val_if_fail (PLUGIN_IS_FLATPAK_ARCH_OPTIONS (self), NULL);

  keys = (char **)g_hash_table_get_keys_as_array (self->arches, NULL);
  for (gsize i = 0; keys[i]; i++)
    keys[i] = g_strdup (keys[i]);

  return g_steal_pointer (&keys);
}

PluginFlatpakOptions *
plugin_flatpak_arch_options_dup_arch (PluginFlatpakArchOptions *self,
                                      const char               *arch)
{
  PluginFlatpakOptions *ret;

  g_return_val_if_fail (PLUGIN_IS_FLATPAK_ARCH_OPTIONS (self), NULL);
  g_return_val_if_fail (arch != NULL, NULL);

  if ((ret = g_hash_table_lookup (self->arches, arch)))
    return g_object_ref (ret);

  return NULL;
}
