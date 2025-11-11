/* plugin-ctags-symbol.c
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

#include "plugin-ctags-file.h"
#include "plugin-ctags-symbol.h"

struct _PluginCtagsSymbol
{
  FoundrySymbol parent_instance;
  PluginCtagsFile *file;
  PluginCtagsMatch match;
};

G_DEFINE_FINAL_TYPE (PluginCtagsSymbol, plugin_ctags_symbol, FOUNDRY_TYPE_SYMBOL)

static void
plugin_ctags_symbol_finalize (GObject *object)
{
  PluginCtagsSymbol *self = PLUGIN_CTAGS_SYMBOL (object);

  g_clear_object (&self->file);

  G_OBJECT_CLASS (plugin_ctags_symbol_parent_class)->finalize (object);
}

static char *
plugin_ctags_symbol_dup_name (FoundrySymbol *symbol)
{
  PluginCtagsSymbol *self = PLUGIN_CTAGS_SYMBOL (symbol);

  return g_strndup (self->match.name, self->match.name_len);
}

static DexFuture *
plugin_ctags_symbol_find_parent_fiber (gpointer data)
{
  PluginCtagsSymbol *self = data;
  PluginCtagsMatch parent_match;
  g_autoptr(PluginCtagsSymbol) parent = NULL;

  g_assert (PLUGIN_IS_CTAGS_SYMBOL (self));

  /* Only certain kinds can have parents */
  if (self->match.kind != PLUGIN_CTAGS_KIND_MEMBER &&
      self->match.kind != PLUGIN_CTAGS_KIND_FUNCTION &&
      self->match.kind != PLUGIN_CTAGS_KIND_VARIABLE &&
      self->match.kind != PLUGIN_CTAGS_KIND_PROTOTYPE)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Symbol kind does not have a parent");

  if (!plugin_ctags_file_find_parent_match (self->file, &self->match, &parent_match))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "No parent found");

  parent = plugin_ctags_symbol_new (self->file, &parent_match);

  return dex_future_new_take_object (g_steal_pointer (&parent));
}

static DexFuture *
plugin_ctags_symbol_find_parent (FoundrySymbol *symbol)
{
  PluginCtagsSymbol *self = PLUGIN_CTAGS_SYMBOL (symbol);

  g_return_val_if_fail (PLUGIN_IS_CTAGS_SYMBOL (self), NULL);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_ctags_symbol_find_parent_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static char *
make_parent_key_from_match (const PluginCtagsMatch *match)
{
  g_autofree char *name_str = NULL;

  name_str = g_strndup (match->name, match->name_len);

  switch (match->kind)
    {
    case PLUGIN_CTAGS_KIND_CLASS_NAME:
      return g_strdup_printf ("class:%s", name_str);

    case PLUGIN_CTAGS_KIND_UNION:
      return g_strdup_printf ("union:%s", name_str);

    case PLUGIN_CTAGS_KIND_STRUCTURE:
      return g_strdup_printf ("struct:%s", name_str);

    case PLUGIN_CTAGS_KIND_IMPORT:
      return g_strdup_printf ("package:%s", name_str);

    case PLUGIN_CTAGS_KIND_FUNCTION:
    case PLUGIN_CTAGS_KIND_MEMBER:
    case PLUGIN_CTAGS_KIND_PROTOTYPE:
      {
        const char *colon;

        if (match->kv != NULL && match->kv_len > 0)
          {
            g_autofree char *kv_str = g_strndup (match->kv, match->kv_len);

            if ((colon = strchr (kv_str, ':')) != NULL)
              return g_strdup_printf ("function:%s.%s", colon + 1, name_str);
          }

        return g_strdup_printf ("function:%s", name_str);
      }

    case PLUGIN_CTAGS_KIND_ENUMERATION_NAME:
      return g_strdup_printf ("enum:%s", name_str);

    case PLUGIN_CTAGS_KIND_ANCHOR:
    case PLUGIN_CTAGS_KIND_DEFINE:
    case PLUGIN_CTAGS_KIND_ENUMERATOR:
    case PLUGIN_CTAGS_KIND_FILE_NAME:
    case PLUGIN_CTAGS_KIND_TYPEDEF:
    case PLUGIN_CTAGS_KIND_VARIABLE:
    default:
      break;
    }

  return NULL;
}

static DexFuture *
plugin_ctags_symbol_list_children_fiber (gpointer data)
{
  PluginCtagsSymbol *self = data;
  g_autofree char *parent_key = NULL;
  g_autoptr(GListStore) store = NULL;
  gsize size;

  g_assert (PLUGIN_IS_CTAGS_SYMBOL (self));

  /* Generate the parent key for this symbol */
  parent_key = make_parent_key_from_match (&self->match);

  if (parent_key == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Symbol kind cannot have children");

  store = g_list_store_new (PLUGIN_TYPE_CTAGS_SYMBOL);
  size = plugin_ctags_file_get_size (self->file);

  for (gsize i = 0; i < size; i++)
    {
      PluginCtagsKind kind;
      const char *kv;
      gsize kv_len;
      g_autofree char *kv_str = NULL;
      g_auto(GStrv) parts = NULL;
      gboolean is_child = FALSE;

      kind = plugin_ctags_file_get_kind (self->file, i);

      /* Only certain kinds can have parents (and thus be children) */
      if (kind != PLUGIN_CTAGS_KIND_MEMBER &&
          kind != PLUGIN_CTAGS_KIND_FUNCTION &&
          kind != PLUGIN_CTAGS_KIND_VARIABLE &&
          kind != PLUGIN_CTAGS_KIND_PROTOTYPE)
        continue;

      plugin_ctags_file_peek_keyval (self->file, i, &kv, &kv_len);

      if (kv == NULL || kv_len == 0)
        continue;

      kv_str = g_strndup (kv, kv_len);
      parts = g_strsplit (kv_str, "\t", 0);

      if (parts == NULL)
        continue;

      /* Check if any part of the kv field matches our parent key */
      for (guint j = 0; parts[j] != NULL; j++)
        {
          if (g_str_equal (parts[j], parent_key))
            {
              is_child = TRUE;
              break;
            }
        }

      if (!is_child)
        continue;

      /* Create symbol and add to store */
      {
        PluginCtagsMatch match;
        gsize name_len;
        gsize path_len;
        gsize pattern_len;

        plugin_ctags_file_peek_name (self->file, i, &match.name, &name_len);
        plugin_ctags_file_peek_path (self->file, i, &match.path, &path_len);
        plugin_ctags_file_peek_pattern (self->file, i, &match.pattern, &pattern_len);
        match.name_len = (guint16)name_len;
        match.path_len = (guint16)path_len;
        match.pattern_len = (guint16)pattern_len;
        match.kv_len = (guint16)kv_len;
        match.kv = kv;
        match.kind = kind;

        {
          g_autoptr(PluginCtagsSymbol) symbol = NULL;

          symbol = plugin_ctags_symbol_new (self->file, &match);

          g_list_store_append (store, symbol);
        }
      }
    }

  return dex_future_new_take_object (G_LIST_MODEL (g_steal_pointer (&store)));
}

static DexFuture *
plugin_ctags_symbol_list_children (FoundrySymbol *symbol)
{
  PluginCtagsSymbol *self = PLUGIN_CTAGS_SYMBOL (symbol);

  g_return_val_if_fail (PLUGIN_IS_CTAGS_SYMBOL (self), NULL);

  return dex_scheduler_spawn (NULL, 0,
                              plugin_ctags_symbol_list_children_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static void
plugin_ctags_symbol_class_init (PluginCtagsSymbolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySymbolClass *symbol_class = FOUNDRY_SYMBOL_CLASS (klass);

  object_class->finalize = plugin_ctags_symbol_finalize;

  symbol_class->dup_name = plugin_ctags_symbol_dup_name;
  symbol_class->find_parent = plugin_ctags_symbol_find_parent;
  symbol_class->list_children = plugin_ctags_symbol_list_children;
}

static void
plugin_ctags_symbol_init (PluginCtagsSymbol *self)
{
}

PluginCtagsSymbol *
plugin_ctags_symbol_new (PluginCtagsFile        *file,
                         const PluginCtagsMatch *match)
{
  PluginCtagsSymbol *self;

  g_return_val_if_fail (PLUGIN_IS_CTAGS_FILE (file), NULL);
  g_return_val_if_fail (match != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_CTAGS_SYMBOL, NULL);
  self->file = g_object_ref (file);
  self->match = *match;

  return self;
}
