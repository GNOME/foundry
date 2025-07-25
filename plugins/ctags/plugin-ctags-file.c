/* plugin-ctags-file.c
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

#include "gtktimsortprivate.h"
#include "line-reader-private.h"

#include "plugin-ctags-file.h"

typedef enum _EntryKind
{
  ENTRY_ANCHOR = 'a',
  ENTRY_CLASS_NAME = 'c',
  ENTRY_DEFINE = 'd',
  ENTRY_ENUMERATOR = 'e',
  ENTRY_FUNCTION = 'f',
  ENTRY_FILE_NAME = 'F',
  ENTRY_ENUMERATION_NAME = 'g',
  ENTRY_IMPORT = 'i',
  ENTRY_MEMBER = 'm',
  ENTRY_PROTOTYPE = 'p',
  ENTRY_STRUCTURE = 's',
  ENTRY_TYPEDEF = 't',
  ENTRY_UNION = 'u',
  ENTRY_VARIABLE = 'v',
} EntryKind;

typedef struct _Entry
{
  guint64 offset : 48;
  guint64 len : 16;
  guint64 kind : 8;
  guint16 name_len;
  guint16 path_len;
  guint16 pattern_len;
  guint16 kv_len;
} Entry;

#define EGG_ARRAY_NAME entries
#define EGG_ARRAY_TYPE_NAME Entries
#define EGG_ARRAY_ELEMENT_TYPE Entry
#include "eggarrayimpl.c"

struct _PluginCtagsFile
{
  GObject     parent_instance;
  GFile      *file;
  GBytes     *bytes;
  const char *base;
  Entries     entries;
};

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginCtagsFile, plugin_ctags_file, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
plugin_ctags_file_finalize (GObject *object)
{
  PluginCtagsFile *self = (PluginCtagsFile *)object;

  entries_clear (&self->entries);
  g_clear_object (&self->file);
  g_clear_pointer (&self->bytes, g_bytes_unref);

  G_OBJECT_CLASS (plugin_ctags_file_parent_class)->finalize (object);
}

static void
plugin_ctags_file_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  PluginCtagsFile *self = PLUGIN_CTAGS_FILE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, plugin_ctags_file_dup_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_ctags_file_class_init (PluginCtagsFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_ctags_file_finalize;
  object_class->get_property = plugin_ctags_file_get_property;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_ctags_file_init (PluginCtagsFile *self)
{
  entries_init (&self->entries);
}

static gboolean
forward_to_tab (const char **iter,
                const char         *endptr)
{
  while (*iter < endptr && g_utf8_get_char (*iter) != '\t')
    *iter = g_utf8_next_char (*iter);
  return *iter < endptr;
}

static gboolean
forward_to_nontab (const char **iter,
                   const char         *endptr)
{
  while (*iter < endptr && (g_utf8_get_char (*iter) == '\t'))
    *iter = g_utf8_next_char (*iter);
  return *iter < endptr;
}

static int
compare_to_tab (const char *s1,
                const char *s2,
                const char *e1,
                const char *e2)
{
  while (*s1 != '\t' && *s2 != '\t' && s1 < e1 && s2 < e2)
    {
      if (*s1 != *s2)
        {
          if (*s1 < *s2)
            return -1;
          else if (*s1 > *s2)
            return 1;
          else
            return 0;
        }

      s1++;
      s2++;
    }

  if ((*s1 == '\t' || s1 == e1) && (*s2 != '\t' && s2 != e2))
    return -1;

  if ((*s1 != '\t' && s1 != e1) && (*s2 == '\t' || s2 == e2))
    return 1;

  return 0;
}

static int
entry_compare (gconstpointer a,
               gconstpointer b,
               gpointer      data)
{
  const char *base = data;
  const Entry *entry_a = a;
  const Entry *entry_b = b;
  const char *end_a = base + entry_a->offset + entry_a->len;
  const char *end_b = base + entry_b->offset + entry_b->len;
  const char *name_a;
  const char *name_b;
  const char *path_a;
  const char *path_b;
  const char *pattern_a;
  const char *pattern_b;
  int rval;

  name_a = base + entry_a->offset;
  name_b = base + entry_b->offset;

  if ((rval = compare_to_tab (name_a, name_b, end_a, end_b)))
    return rval;

  if (entry_a->kind < entry_b->kind)
    return -1;
  else if (entry_a->kind > entry_b->kind)
    return 1;

  pattern_a = base + entry_a->offset + entry_a->name_len + entry_a->path_len;
  pattern_b = base + entry_b->offset + entry_b->name_len + entry_b->path_len;

  if ((rval = compare_to_tab (pattern_a, pattern_b, end_a, end_b)))
    return rval;

  path_a = base + entry_a->offset + entry_a->name_len;
  path_b = base + entry_b->offset + entry_b->name_len;

  if ((rval = compare_to_tab (path_a, path_b, end_a, end_b)))
    return rval;

  return 0;
}

static DexFuture *
plugin_ctags_file_new_fiber (gpointer data)
{
  GFile *file = data;
  g_autoptr(PluginCtagsFile) self = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  const char *start;
  const char *line;
  LineReader reader;
  gsize line_len;

  g_assert (G_IS_FILE (file));

  if (g_file_is_native (file))
    {
      g_autoptr(GMappedFile) mapped_file = NULL;
      g_autofree char *path = g_file_get_path (file);

      if ((mapped_file = g_mapped_file_new (path, FALSE, NULL)))
        bytes = g_mapped_file_get_bytes (mapped_file);
    }

  if (bytes == NULL)
    {
      if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (g_bytes_get_size (bytes) > ((1L << 48) - 1))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "File is too large");

  self = g_object_new (PLUGIN_TYPE_CTAGS_FILE, NULL);
  self->file = g_object_ref (file);
  self->bytes = g_bytes_ref (bytes);
  self->base = g_bytes_get_data (bytes, NULL);

  start = g_bytes_get_data (bytes, NULL);
  line_reader_init_from_bytes (&reader, bytes);

  while ((line = line_reader_next (&reader, &line_len)))
    {
      Entry entry = {0};
      const char *endptr = &line[line_len];
      const char *iter = line;
      const char *save;

      if (line[0] == '!' || line_len >= G_MAXUINT16)
        continue;

      entry.offset = (iter - start);
      entry.len = line_len;
      save = iter;

      if (!forward_to_tab (&iter, endptr) ||
          !forward_to_nontab (&iter, endptr))
        continue;

      entry.name_len = iter - save, save = iter;

      if (!forward_to_tab (&iter, endptr) ||
          !forward_to_nontab (&iter, endptr))
        continue;

      entry.path_len = iter - save, save = iter;

      if (!forward_to_tab (&iter, endptr) ||
          !forward_to_nontab (&iter, endptr))
        continue;

      entry.pattern_len = iter - save, save = iter;

      if (!forward_to_tab (&iter, endptr) ||
          !forward_to_nontab (&iter, endptr))
        continue;

      entry.kv_len = iter - save, save = iter;
      entry.kind = *iter;

      g_assert (entry.name_len + entry.path_len + entry.pattern_len + entry.kv_len <= line_len);

      entries_append (&self->entries, entry);
    }

  gtk_tim_sort (self->entries.start,
                entries_get_size (&self->entries),
                sizeof (Entry),
                entry_compare,
                (char *)g_bytes_get_data (bytes, NULL));

  return dex_future_new_take_object (g_steal_pointer (&self));
}

DexFuture *
plugin_ctags_file_new (GFile *file)
{
  dex_return_error_if_fail (G_IS_FILE (file));

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              plugin_ctags_file_new_fiber,
                              g_object_ref (file),
                              g_object_unref);
}

GFile *
plugin_ctags_file_dup_file (PluginCtagsFile *self)
{
  g_return_val_if_fail (PLUGIN_IS_CTAGS_FILE (self), NULL);

  return g_object_ref (self->file);
}

gsize
plugin_ctags_file_get_size (PluginCtagsFile *self)
{
  g_return_val_if_fail (PLUGIN_IS_CTAGS_FILE (self), 0);

  return entries_get_size (&self->entries);
}

void
plugin_ctags_file_peek_name (PluginCtagsFile  *self,
                             gsize             position,
                             const char      **name,
                             gsize            *name_len)
{
  const Entry *entry;
  const char *endptr;
  gsize len = 0;

  g_assert (PLUGIN_IS_CTAGS_FILE (self));
  g_assert (position < entries_get_size (&self->entries));
  g_assert (name != NULL);
  g_assert (name_len != NULL);

  entry = entries_index (&self->entries, position);

  *name = self->base + entry->offset;
  endptr = self->base + entry->offset + entry->len;

  for (const char *c = *name; c < endptr; c = g_utf8_next_char (c))
    {
      if (g_unichar_isspace (g_utf8_get_char (c)))
        break;
      len++;
    }

  *name_len = len;
}

char *
plugin_ctags_file_dup_name (PluginCtagsFile *self,
                            gsize            position)
{
  const char *str;
  gsize len;

  plugin_ctags_file_peek_name (self, position, &str, &len);

  return g_strndup (str, len);
}

void
plugin_ctags_file_peek_path (PluginCtagsFile  *self,
                             gsize             position,
                             const char      **path,
                             gsize            *path_len)
{
  const Entry *entry;
  const char *endptr;
  gsize len = 0;

  g_assert (PLUGIN_IS_CTAGS_FILE (self));
  g_assert (position < entries_get_size (&self->entries));
  g_assert (path != NULL);
  g_assert (path_len != NULL);

  entry = entries_index (&self->entries, position);

  *path = self->base + entry->offset + entry->name_len;
  endptr = self->base + entry->offset + entry->len;

  for (const char *c = *path; c < endptr; c = g_utf8_next_char (c))
    {
      if (g_unichar_isspace (g_utf8_get_char (c)))
        break;
      len++;
    }

  *path_len = len;
}

char *
plugin_ctags_file_dup_path (PluginCtagsFile *self,
                            gsize            position)
{
  const char *str;
  gsize len;

  plugin_ctags_file_peek_path (self, position, &str, &len);

  return g_strndup (str, len);
}

void
plugin_ctags_file_peek_pattern (PluginCtagsFile  *self,
                                gsize             position,
                                const char      **pattern,
                                gsize            *pattern_len)
{
  const Entry *entry;
  const char *endptr;
  gsize len = 0;

  g_assert (PLUGIN_IS_CTAGS_FILE (self));
  g_assert (position < entries_get_size (&self->entries));
  g_assert (pattern != NULL);
  g_assert (pattern_len != NULL);

  entry = entries_index (&self->entries, position);

  *pattern = self->base + entry->offset + entry->name_len + entry->path_len;
  endptr = self->base + entry->offset + entry->len;

  for (const char *c = *pattern; c < endptr; c = g_utf8_next_char (c))
    {
      if (g_unichar_isspace (g_utf8_get_char (c)))
        break;
      len++;
    }

  *pattern_len = len;
}

char *
plugin_ctags_file_dup_pattern (PluginCtagsFile *self,
                               gsize            position)
{
  const char *str;
  gsize len;

  plugin_ctags_file_peek_pattern (self, position, &str, &len);

  return g_strndup (str, len);
}

void
plugin_ctags_file_peek_keyval (PluginCtagsFile  *self,
                               gsize             position,
                               const char      **keyval,
                               gsize            *keyval_len)
{
  const Entry *entry;
  const char *endptr;
  gsize len = 0;

  g_assert (PLUGIN_IS_CTAGS_FILE (self));
  g_assert (position < entries_get_size (&self->entries));
  g_assert (keyval != NULL);
  g_assert (keyval_len != NULL);

  entry = entries_index (&self->entries, position);

  *keyval = self->base + entry->offset + entry->name_len + entry->path_len + entry->pattern_len;
  endptr = self->base + entry->offset + entry->len;

  for (const char *c = *keyval; c < endptr; c = g_utf8_next_char (c))
    {
      if (g_unichar_isspace (g_utf8_get_char (c)))
        break;
      len++;
    }

  *keyval_len = len;
}

char *
plugin_ctags_file_dup_keyval (PluginCtagsFile *self,
                              gsize            position)
{
  const char *str;
  gsize len;

  plugin_ctags_file_peek_keyval (self, position, &str, &len);

  return g_strndup (str, len);
}
