/* plugin-meson-test.c
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

#include "plugin-meson-test.h"

struct _PluginMesonTest
{
  FoundryTest parent_instance;
  JsonNode *node;
};

G_DEFINE_FINAL_TYPE (PluginMesonTest, plugin_meson_test, FOUNDRY_TYPE_TEST)

static inline gboolean
get_string_member (JsonObject  *object,
                   const char  *member,
                   char       **location)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);

  g_clear_pointer (location, g_free);

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_VALUE (node))
    {
      *location = g_strdup (json_node_get_string (node));
      return TRUE;
    }

  return FALSE;
}

static inline gboolean
get_strv_member (JsonObject   *object,
                 const char   *member,
                 char       ***location)
{
  JsonNode *node;
  JsonArray *ar;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);

  g_clear_pointer (location, g_strfreev);

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_ARRAY (node) &&
      (ar = json_node_get_array (node)))
    {
      GPtrArray *strv = g_ptr_array_new ();
      guint n_items = json_array_get_length (ar);

      for (guint i = 0; i < n_items; i++)
        {
          JsonNode *ele = json_array_get_element (ar, i);
          const char *str;

          if (JSON_NODE_HOLDS_VALUE (ele) &&
              (str = json_node_get_string (ele)))
            g_ptr_array_add (strv, g_strdup (str));
        }

      g_ptr_array_add (strv, NULL);

      *location = (char **)(gpointer)g_ptr_array_free (strv, FALSE);

      return TRUE;
    }

  return FALSE;
}

static inline gboolean
get_environ_member (JsonObject   *object,
                    const char   *member,
                    char       ***location)
{
  JsonNode *node;
  JsonObject *envobj;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);
  g_assert (*location == NULL);

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_OBJECT (node) &&
      (envobj = json_node_get_object (node)))
    {
      JsonObjectIter iter;
      const char *key;
      JsonNode *value_node;

      json_object_iter_init (&iter, envobj);
      while (json_object_iter_next (&iter, &key, &value_node))
        {
          const char *value;

          if (!JSON_NODE_HOLDS_VALUE (value_node) ||
              !(value = json_node_get_string (value_node)))
            continue;

          *location = g_environ_setenv (*location, key, value, TRUE);
        }

      return TRUE;
    }

  return FALSE;
}

static char *
plugin_meson_test_dup_id (FoundryTest *test)
{
  PluginMesonTest *self = PLUGIN_MESON_TEST (test);
  g_autofree char *name = NULL;

  if (get_string_member (json_node_get_object (self->node), "name", &name))
    return g_steal_pointer (&name);

  return NULL;
}

static char *
plugin_meson_test_dup_title (FoundryTest *test)
{
  return plugin_meson_test_dup_id (test);
}

static void
plugin_meson_test_finalize (GObject *object)
{
  PluginMesonTest *self = (PluginMesonTest *)object;

  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_meson_test_parent_class)->finalize (object);
}

static void
plugin_meson_test_class_init (PluginMesonTestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryTestClass *test_class = FOUNDRY_TEST_CLASS (klass);

  object_class->finalize = plugin_meson_test_finalize;

  test_class->dup_id = plugin_meson_test_dup_id;
  test_class->dup_title = plugin_meson_test_dup_title;
}

static void
plugin_meson_test_init (PluginMesonTest *self)
{
}

PluginMesonTest *
plugin_meson_test_new (FoundryContext *context,
                       JsonNode       *node)
{
  PluginMesonTest *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (node != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_MESON_TEST,
                       "context", context,
                       NULL);
  self->node = json_node_ref (node);

  return g_steal_pointer (&self);
}
