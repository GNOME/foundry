/* foundry-file-navigator.c
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

#include <foundry.h>

#include "foundry-file-navigator.h"

struct _FoundryFileNavigator
{
  FoundryPathNavigator  parent_instance;
  GFile                *file;
};

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryFileNavigator, foundry_file_navigator, FOUNDRY_TYPE_PATH_NAVIGATOR)

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_file_navigator_list_directory_fiber (gpointer data)
{
  g_autoptr(GFile) directory = data;
  g_autoptr(FoundryDirectoryListing) listing = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GError) error = NULL;
  GFileType file_type;
  guint n_items;

  g_assert (G_IS_FILE (directory));

  file_type = dex_await_enum (dex_file_query_file_type (directory, 0, 0), &error);

  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (file_type != G_FILE_TYPE_DIRECTORY)
    return foundry_future_new_not_supported ();

  listing = foundry_directory_listing_new (NULL,
                                           directory,
                                           G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                           G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                           G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                           G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE ","
                                           G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON ",",
                                           G_FILE_QUERY_INFO_NONE);

  /* We don't do live tracking, so just wait for it to populate
   * and then we'll create our listing from that.
   */
  if (!dex_await (foundry_directory_listing_await (listing), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  store = g_list_store_new (FOUNDRY_TYPE_FILE_NAVIGATOR);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (listing));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDirectoryItem) item = g_list_model_get_item (G_LIST_MODEL (listing), i);
      g_autoptr(GFile) file = foundry_directory_item_dup_file (item);
      FoundryFileNavigator *navigator = foundry_file_navigator_new (file);

      g_list_store_append (store, navigator);
    }

  return dex_future_new_for_object (g_steal_pointer (&store));
}

static DexFuture *
foundry_file_navigator_find_parent (FoundryPathNavigator *navigator)
{
  FoundryFileNavigator *self = FOUNDRY_FILE_NAVIGATOR (navigator);
  g_autoptr(GFile) parent = NULL;

  g_assert (FOUNDRY_IS_FILE_NAVIGATOR (self));

  if (!(parent = g_file_get_parent (self->file)))
    return dex_future_new_for_object (NULL);

  return dex_future_new_for_object (foundry_file_navigator_new (parent));
}

static DexFuture *
foundry_file_navigator_list_children (FoundryPathNavigator *navigator)
{
  FoundryFileNavigator *self = FOUNDRY_FILE_NAVIGATOR (navigator);

  g_assert (FOUNDRY_IS_FILE_NAVIGATOR (self));

  return dex_scheduler_spawn (NULL, 0,
                              foundry_file_navigator_list_directory_fiber,
                              g_object_ref (self->file),
                              g_object_unref);
}

static DexFuture *
foundry_file_navigator_list_siblings (FoundryPathNavigator *navigator)
{
  FoundryFileNavigator *self = FOUNDRY_FILE_NAVIGATOR (navigator);
  g_autoptr(GFile) parent = NULL;

  g_assert (FOUNDRY_IS_FILE_NAVIGATOR (self));

  if (!(parent = g_file_get_parent (self->file)))
    {
      g_autoptr(GListStore) store = NULL;

      store = g_list_store_new (FOUNDRY_TYPE_FILE_NAVIGATOR);
      g_list_store_append (store, g_object_ref (self));

      return dex_future_new_for_object (g_steal_pointer (&store));
    }

  return dex_scheduler_spawn (NULL, 0,
                              foundry_file_navigator_list_directory_fiber,
                              g_steal_pointer (&parent),
                              g_object_unref);
}

static char *
foundry_file_navigator_dup_title (FoundryPathNavigator *navigator)
{
  FoundryFileNavigator *self = FOUNDRY_FILE_NAVIGATOR (navigator);
  g_autofree char *name = NULL;

  g_assert (FOUNDRY_IS_FILE_NAVIGATOR (self));

  name = g_file_get_basename (self->file);

  return g_filename_display_name (name);
}

static GIcon *
foundry_file_navigator_dup_icon (FoundryPathNavigator *navigator)
{
  FoundryFileNavigator *self = FOUNDRY_FILE_NAVIGATOR (navigator);
  g_autofree char *name = NULL;

  g_assert (FOUNDRY_IS_FILE_NAVIGATOR (self));

  name = g_file_get_basename (self->file);

  return foundry_file_manager_find_symbolic_icon (NULL, NULL, name);
}

static FoundryIntent *
foundry_file_navigator_dup_intent (FoundryPathNavigator *navigator)
{
  FoundryFileNavigator *self = (FoundryFileNavigator *)navigator;

  g_assert (FOUNDRY_IS_FILE_NAVIGATOR (self));

  return foundry_open_file_intent_new (self->file, NULL);
}

static void
foundry_file_navigator_dispose (GObject *object)
{
  FoundryFileNavigator *self = FOUNDRY_FILE_NAVIGATOR (object);

  g_clear_object (&self->file);

  G_OBJECT_CLASS (foundry_file_navigator_parent_class)->dispose (object);
}

static void
foundry_file_navigator_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundryFileNavigator *self = FOUNDRY_FILE_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, foundry_file_navigator_dup_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_navigator_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  FoundryFileNavigator *self = FOUNDRY_FILE_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_FILE:
      self->file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_navigator_class_init (FoundryFileNavigatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryPathNavigatorClass *path_navigator_class = FOUNDRY_PATH_NAVIGATOR_CLASS (klass);

  object_class->dispose = foundry_file_navigator_dispose;
  object_class->get_property = foundry_file_navigator_get_property;
  object_class->set_property = foundry_file_navigator_set_property;

  path_navigator_class->find_parent = foundry_file_navigator_find_parent;
  path_navigator_class->list_children = foundry_file_navigator_list_children;
  path_navigator_class->list_siblings = foundry_file_navigator_list_siblings;
  path_navigator_class->dup_title = foundry_file_navigator_dup_title;
  path_navigator_class->dup_icon = foundry_file_navigator_dup_icon;
  path_navigator_class->dup_intent = foundry_file_navigator_dup_intent;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_file_navigator_init (FoundryFileNavigator *self)
{
}

/**
 * foundry_file_navigator_new:
 * @file: a [class@Gio.File]
 *
 * Creates a new file navigator for the given file.
 *
 * Returns: (transfer full): a new [class@Foundry.FileNavigator]
 *
 * Since: 1.1
 */
FoundryFileNavigator *
foundry_file_navigator_new (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_object_new (FOUNDRY_TYPE_FILE_NAVIGATOR,
                       "file", file,
                       NULL);
}

/**
 * foundry_file_navigator_dup_file:
 * @self: a [class@Foundry.FileNavigator]
 *
 * Gets the file for this navigator.
 *
 * Returns: (transfer full): a [class@Gio.File]
 *
 * Since: 1.1
 */
GFile *
foundry_file_navigator_dup_file (FoundryFileNavigator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FILE_NAVIGATOR (self), NULL);

  return g_object_ref (self->file);
}
