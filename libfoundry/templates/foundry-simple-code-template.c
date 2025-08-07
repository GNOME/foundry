/* foundry-simple-code-template.c
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

#include <glib/gi18n-lib.h>

#include <tmpl-glib.h>

#include "foundry-context.h"
#include "foundry-input-file.h"
#include "foundry-input-group.h"
#include "foundry-input-switch.h"
#include "foundry-input-text.h"
#include "foundry-license.h"
#include "foundry-simple-code-template.h"
#include "foundry-template-locator-private.h"
#include "foundry-util.h"

#include "line-reader-private.h"

#include "../../plugins/shared/templates.h"

struct _FoundrySimpleCodeTemplate
{
  FoundryCodeTemplate parent_instance;
  FoundryInput *input;
  FoundryInput *location;
  char *title;
  char *id;
  GHashTable *files;
};

G_DEFINE_FINAL_TYPE (FoundrySimpleCodeTemplate, foundry_simple_code_template, FOUNDRY_TYPE_CODE_TEMPLATE)

static GRegex *input_text;
static GRegex *input_switch;

#define has_prefix(str,len,prefix) \
  ((len >= strlen(prefix)) && (memcmp(str,prefix,strlen(prefix)) == 0))

static char *
foundry_simple_code_template_dup_id (FoundryTemplate *template)
{
  return g_strdup (FOUNDRY_SIMPLE_CODE_TEMPLATE (template)->id);
}

static char *
foundry_simple_code_template_dup_description (FoundryTemplate *template)
{
  return g_strdup (FOUNDRY_SIMPLE_CODE_TEMPLATE (template)->title);
}

static FoundryInput *
foundry_simple_code_template_dup_input (FoundryTemplate *template)
{
  FoundrySimpleCodeTemplate *self = FOUNDRY_SIMPLE_CODE_TEMPLATE (template);

  return self->input ? g_object_ref (self->input) : NULL;
}

static DexFuture *
foundry_simple_code_template_expand_fiber (FoundrySimpleCodeTemplate *self,
                                           FoundryLicense            *license)
{
  g_autoptr(TmplTemplateLocator) locator = NULL;
  g_autoptr(GFile) location = NULL;
  GHashTableIter iter;
  gpointer k, v;

  g_assert (FOUNDRY_IS_SIMPLE_CODE_TEMPLATE (self));
  g_assert (!license || FOUNDRY_IS_LICENSE (license));

  location = foundry_input_file_dup_value (FOUNDRY_INPUT_FILE (self->location));
  locator = foundry_template_locator_new ();

  if (license != NULL)
    {
      g_autoptr(GBytes) license_bytes = foundry_license_dup_snippet_text (license);

      foundry_template_locator_set_license_text (FOUNDRY_TEMPLATE_LOCATOR (locator), license_bytes);
    }

  g_hash_table_iter_init (&iter, self->files);

  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      g_autoptr(TmplScope) scope = tmpl_scope_new ();
      g_autoptr(TmplTemplate) template = NULL;
      g_autoptr(GListModel) children = NULL;
      g_autoptr(GFile) dest_file = NULL;
      g_autoptr(GFile) dest_dir = NULL;
      g_autoptr(GError) error = NULL;
      g_autoptr(GBytes) output = NULL;
      g_autofree char *expand = NULL;
      g_autofree char *pattern = g_strdup ((char *)k);
      g_auto(GValue) return_value = G_VALUE_INIT;
      GBytes *bytes = v;
      guint n_items;

      add_simple_scope (scope);

      children = foundry_input_group_list_children (FOUNDRY_INPUT_GROUP (self->input));
      n_items = g_list_model_get_n_items (children);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(FoundryInput) child = g_list_model_get_item (children, i);
          g_autofree char *name = foundry_input_dup_title (child);
          GObjectClass *klass = G_OBJECT_GET_CLASS (child);
          g_auto(GValue) value = G_VALUE_INIT;
          GParamSpec *pspec;

          if ((pspec = g_object_class_find_property (klass, "value")))
            {
              g_value_init (&value, pspec->value_type);
              g_object_get_property (G_OBJECT (child), "value", &value);
            }
          else if ((pspec = g_object_class_find_property (klass, "choice")))
            {
              g_value_init (&value, pspec->value_type);
              g_object_get_property (G_OBJECT (child), "choice", &value);
            }

          if (G_IS_VALUE (&value))
            tmpl_scope_set_value (scope, name, &value);
        }

      if (strstr (pattern, "{{"))
        {
          g_autoptr(TmplTemplate) expander = tmpl_template_new (NULL);
          g_autofree char *dest_eval = NULL;

          if (!tmpl_template_parse_string (expander, pattern, &error))
            return dex_future_new_for_error (g_steal_pointer (&error));

          if (!(dest_eval = tmpl_template_expand_string (expander, scope, &error)))
            return dex_future_new_for_error (g_steal_pointer (&error));

          g_set_str (&pattern, dest_eval);
        }

      dest_file = g_file_get_child (location, pattern);
      dest_dir = g_file_get_parent (dest_file);

      if (!g_file_has_prefix (dest_file, location))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVAL,
                                      "Cannot create file above location");

      if (!dex_await (dex_file_make_directory_with_parents (dest_dir), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      tmpl_scope_set_string (scope, "filename", pattern);

      template = tmpl_template_new (locator);

      if (!tmpl_template_parse_string (template, (char *)g_bytes_get_data (bytes, NULL), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(expand = tmpl_template_expand_string (template, scope, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      output = g_bytes_new_take (expand, strlen (expand)), expand = NULL;

      if (!dex_await (dex_file_replace_contents_bytes (dest_file, output, NULL, FALSE, 0), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
foundry_simple_code_template_expand (FoundryTemplate *template)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryLicense) default_license = NULL;

  g_assert (FOUNDRY_IS_SIMPLE_CODE_TEMPLATE (template));

  if ((context = foundry_code_template_dup_context (FOUNDRY_CODE_TEMPLATE (template))))
    default_license = foundry_context_dup_default_license (context);

  return foundry_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                                  G_CALLBACK (foundry_simple_code_template_expand_fiber),
                                  2,
                                  FOUNDRY_TYPE_SIMPLE_CODE_TEMPLATE, template,
                                  FOUNDRY_TYPE_LICENSE, default_license);
}

static void
foundry_simple_code_template_finalize (GObject *object)
{
  FoundrySimpleCodeTemplate *self = (FoundrySimpleCodeTemplate *)object;

  g_clear_object (&self->input);
  g_clear_object (&self->location);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->files, g_hash_table_unref);

  G_OBJECT_CLASS (foundry_simple_code_template_parent_class)->finalize (object);
}

static void
foundry_simple_code_template_class_init (FoundrySimpleCodeTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryTemplateClass *template_class = FOUNDRY_TEMPLATE_CLASS (klass);
  g_autoptr(GError) error = NULL;

  object_class->finalize = foundry_simple_code_template_finalize;

  template_class->dup_id = foundry_simple_code_template_dup_id;
  template_class->dup_description = foundry_simple_code_template_dup_description;
  template_class->dup_input = foundry_simple_code_template_dup_input;
  template_class->expand = foundry_simple_code_template_expand;

  input_text = g_regex_new ("Input\\[([_\\w]+)\\]:\\s*\"(.*)\"", G_REGEX_OPTIMIZE, 0, &error);
  g_assert_no_error (error);

  input_switch = g_regex_new ("Input\\[([_\\w]+)\\]:\\s*\(true|false)", G_REGEX_OPTIMIZE, 0, &error);
  g_assert_no_error (error);
}

static void
foundry_simple_code_template_init (FoundrySimpleCodeTemplate *self)
{
  self->files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_bytes_unref);
}

static gboolean
parse_input (GPtrArray   *inputs,
             const char  *line,
             gsize        len,
             const char  *basename,
             guint        lineno,
             GError     **error)
{
  g_autoptr(GMatchInfo) match_info = NULL;

  g_assert (inputs != NULL);
  g_assert (g_str_has_prefix (line, "Input["));
  g_assert (basename != NULL);

  if (g_regex_match_full (input_text, line, len, 0, 0, &match_info, NULL))
    {
      if (g_match_info_matches (match_info))
        {
          g_autofree char *name = g_match_info_fetch (match_info, 1);
          g_autofree char *value = g_match_info_fetch (match_info, 2);

          g_ptr_array_add (inputs,
                           foundry_input_text_new (name, NULL, NULL, value));
          return TRUE;
        }
    }

  g_clear_pointer (&match_info, g_match_info_free);

  if (g_regex_match_full (input_switch, line, len, 0, 0, &match_info, NULL))
    {
      if (g_match_info_matches (match_info))
        {
          g_autofree char *name = g_match_info_fetch (match_info, 1);
          g_autofree char *value = g_match_info_fetch (match_info, 2);

          g_ptr_array_add (inputs,
                           foundry_input_switch_new (name, NULL, NULL, value && value[0] == 't'));
          return TRUE;
        }
    }

  g_clear_pointer (&match_info, g_match_info_free);

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_DATA,
               "Invalid template at `%s:%u`",
               basename, lineno);

  return FALSE;
}

static DexFuture *
foundry_simple_code_template_new_fiber (FoundryContext *context,
                                        GFile          *file)
{
  g_autoptr(FoundrySimpleCodeTemplate) self = NULL;
  g_autoptr(GPtrArray) inputs = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) location = NULL;
  g_autofree char *basename = NULL;
  g_autofree char *title = NULL;
  LineReader reader;
  guint lineno = 0;
  char *line;
  gsize len;

  g_assert (!context || FOUNDRY_IS_CONTEXT (context));
  g_assert (G_IS_FILE (file));

  inputs = g_ptr_array_new_with_free_func (g_object_unref);
  basename = g_file_get_basename (file);
  self = g_object_new (FOUNDRY_TYPE_SIMPLE_CODE_TEMPLATE,
                       "context", context,
                       NULL);

  if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if ((context = foundry_code_template_dup_context (FOUNDRY_CODE_TEMPLATE (self))))
    location = foundry_context_dup_project_directory (context);
  else
    location = g_file_new_for_path (g_get_current_dir ());

  self->location = foundry_input_file_new (_("Location"), NULL, NULL,
                                           G_FILE_TYPE_DIRECTORY,
                                           location);
  g_ptr_array_add (inputs, g_object_ref (self->location));

  line_reader_init_from_bytes (&reader, bytes);

  /* read input variables until blank line */
  while ((line = line_reader_next (&reader, &len)))
    {
      lineno++;

      if (len == 0)
        break;

      if (line[0] == '#')
        continue;

      if (has_prefix (line, len, "Title:"))
        {
          foundry_take_str (&title,
                            g_strstrip (g_strndup (line + strlen ("Title:"), len - strlen ("Title:"))));
          continue;
        }

      if (!parse_input (inputs, line, len, basename, lineno, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  while ((line = line_reader_next (&reader, &len)))
    {
      lineno++;

      if (has_prefix (line, len, "```"))
        {
          g_autofree char *file_pattern = g_strndup (line + 3, len - 3);
          g_autoptr(GString) str = g_string_new (NULL);

          while ((line = line_reader_next (&reader, &len)))
            {
              lineno++;

              if (has_prefix (line, len, "```"))
                break;

              g_string_append_len (str, line, len);
              g_string_append_c (str, '\n');
            }

          g_hash_table_replace (self->files,
                                g_steal_pointer (&file_pattern),
                                g_string_free_to_bytes (g_steal_pointer (&str)));
        }
    }

  self->input = foundry_input_group_new (title, NULL, NULL,
                                         (FoundryInput **)(gpointer)inputs->pdata,
                                         inputs->len);

  if (g_str_has_suffix (basename, ".template"))
    self->id = g_strndup (basename, strlen (basename) - strlen (".template"));
  else
    self->id = g_strdup (basename);

  self->title = g_strdup (title);

  return dex_future_new_take_object (g_steal_pointer (&self));
}

DexFuture *
foundry_simple_code_template_new (FoundryContext *context,
                                  GFile          *file)
{
  dex_return_error_if_fail (!context || FOUNDRY_IS_CONTEXT (context));
  dex_return_error_if_fail (G_IS_FILE (file));

  return foundry_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                                  G_CALLBACK (foundry_simple_code_template_new_fiber),
                                  2,
                                  FOUNDRY_TYPE_CONTEXT, context,
                                  G_TYPE_FILE, file);
}
