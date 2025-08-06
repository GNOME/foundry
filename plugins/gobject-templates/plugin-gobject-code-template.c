/* plugin-gobject-code-template.c
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

#include "plugin-gobject-code-template.h"

#include "../meson-templates/plugin-meson-template-locator.h"
#include "../shared/templates.h"

struct _PluginGobjectCodeTemplate
{
  FoundryCodeTemplate                  parent_instance;
  const PluginGobjectCodeTemplateInfo *info;
  FoundryInput                        *input;
};

G_DEFINE_FINAL_TYPE (PluginGobjectCodeTemplate, plugin_gobject_code_template, FOUNDRY_TYPE_CODE_TEMPLATE)

static char *
plugin_gobject_code_template_dup_id (FoundryTemplate *template)
{
  PluginGobjectCodeTemplate *self = PLUGIN_GOBJECT_CODE_TEMPLATE (template);

  return g_strdup (self->info->id);
}

static char *
plugin_gobject_code_template_dup_description (FoundryTemplate *template)
{
  PluginGobjectCodeTemplate *self = PLUGIN_GOBJECT_CODE_TEMPLATE (template);

  return g_strdup (self->info->description);
}

static FoundryInput *
plugin_gobject_code_template_dup_input (FoundryTemplate *template)
{
  PluginGobjectCodeTemplate *self = PLUGIN_GOBJECT_CODE_TEMPLATE (template);

  g_assert (PLUGIN_IS_GOBJECT_CODE_TEMPLATE (self));

  if (self->info->n_inputs == 0)
    return NULL;

  if (self->input == NULL)
    {
      g_autoptr(GPtrArray) input = g_ptr_array_new_with_free_func (g_object_unref);
      g_autoptr(FoundryContext) context = foundry_code_template_dup_context (FOUNDRY_CODE_TEMPLATE (self));
      g_autoptr(GFile) location = NULL;

      if (context != NULL)
        location = foundry_context_dup_project_directory (context);
      else
        location = g_file_new_for_path (g_get_current_dir ());

      g_ptr_array_add (input,
                       foundry_input_file_new (_("Location"), NULL, NULL,
                                               G_FILE_TYPE_DIRECTORY,
                                               location));

      for (guint i = 0; i < self->info->n_inputs; i++)
        {
          const PluginGobjectCodeTemplateInput *info = &self->info->inputs[i];

          if (info->input_kind == INPUT_KIND_TEXT)
            {
              g_autoptr(GRegex) regex = NULL;
              FoundryInputValidator *validator = NULL;

              if (info->regex != NULL)
                {
                  regex = g_regex_new (info->regex, G_REGEX_OPTIMIZE, 0, NULL);
                  validator = foundry_input_validator_regex_new (regex);
                }

              g_ptr_array_add (input,
                               foundry_input_text_new (info->title, info->subtitle, validator, info->value));
            }
          else if (info->input_kind == INPUT_KIND_SWITCH)
            {
              g_ptr_array_add (input,
                               foundry_input_switch_new (info->title, info->subtitle, NULL,
                                                         info->value && info->value[0] == 't'));
            }
          else g_assert_not_reached ();
        }

      self->input = foundry_input_group_new (NULL, NULL, NULL,
                                             (FoundryInput **)input->pdata,
                                             input->len);
    }

  return g_object_ref (self->input);
}

static DexFuture *
plugin_gobject_code_template_expand_fiber (gpointer data)
{
  PluginGobjectCodeTemplate *self = data;
  g_autoptr(TmplTemplateLocator) locator = NULL;
  g_autoptr(FoundryInputFile) input_location = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryInput) input = NULL;
  g_autoptr(GListModel) children = NULL;
  g_autoptr(TmplScope) scope = NULL;
  g_autoptr(GFile) location = NULL;
  g_autofree char *file_base = NULL;
  guint n_items;

  g_assert (PLUGIN_IS_GOBJECT_CODE_TEMPLATE (self));

  scope = tmpl_scope_new ();

  context = foundry_code_template_dup_context (FOUNDRY_CODE_TEMPLATE (self));
  input = plugin_gobject_code_template_dup_input (FOUNDRY_TEMPLATE (self));
  children = foundry_input_group_list_children (FOUNDRY_INPUT_GROUP (input));
  n_items = g_list_model_get_n_items (children);

  g_assert (n_items == self->info->n_inputs + 1);

  for (guint i = 1; i < n_items; i++)
    {
      const PluginGobjectCodeTemplateInput *info = &self->info->inputs[i-1];
      g_autoptr(FoundryInput) child = g_list_model_get_item (children, i);
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

      if (g_str_equal (info->id, "filename") && G_VALUE_HOLDS_STRING (&value))
        g_set_str (&file_base, g_value_get_string (&value));

      tmpl_scope_set_value (scope, info->id, &value);
    }

  {
    g_autofree char *parent = tmpl_scope_dup_string (scope, "parentclass");
    g_autofree char *ns = tmpl_scope_dup_string (scope, "namespace");
    g_autofree char *cls = tmpl_scope_dup_string (scope, "classname");
    g_autofree char *Type = g_strconcat (ns, cls, NULL);
    g_autofree char *cls_f = functify (cls);
    g_autofree char *ns_f = functify (ns);

    scope_take_string (scope, "PREFIX", g_utf8_strup (ns, -1));
    scope_take_string (scope, "CLASS", g_utf8_strup (cls_f, -1));
    tmpl_scope_set_string (scope, "prefix_", ns_f);
    tmpl_scope_set_string (scope, "class_", cls_f);
    tmpl_scope_set_string (scope, "Prefix", ns);
    tmpl_scope_set_string (scope, "Class", cls);
    tmpl_scope_set_string (scope, "Parent", parent);
  }

  locator = plugin_meson_template_locator_new ();

  if (context != NULL)
    {
      g_autoptr(FoundrySettings) settings = foundry_context_load_project_settings (context);
      g_autoptr(GListModel) licenses = foundry_license_list_all ();
      g_autofree char *default_license = foundry_settings_get_string (settings, "default-license");
      g_autoptr(FoundryLicense) license = foundry_license_find (default_license);

      if (license != NULL)
        {
          g_autoptr(GBytes) license_text = foundry_license_dup_snippet_text (license);

          plugin_meson_template_locator_set_license_text (PLUGIN_MESON_TEMPLATE_LOCATOR (locator), license_text);
        }
    }

  input_location = g_list_model_get_item (children, 0);
  location = foundry_input_file_dup_value (input_location);

  for (guint i = 0; i < self->info->n_files; i++)
    {
      const PluginGobjectCodeTemplateFile *info = &self->info->files[i];
      g_autoptr(TmplTemplate) expander = tmpl_template_new (locator);
      g_autoptr(GError) error = NULL;
      g_autoptr(GBytes) bytes = NULL;
      g_autoptr(GFile) dest_file = NULL;
      g_autoptr(GFile) dest_file_dir = NULL;
      g_autofree char *path = g_strdup_printf ("/app/devsuite/foundry/plugins/gobject-templates/%s", info->resource);
      g_autofree char *dest_base = g_strdup_printf ("%s%s", file_base, info->suffix);
      g_autofree char *expand = NULL;

      if (!tmpl_template_parse_resource (expander, path, NULL, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(expand = tmpl_template_expand_string (expander, scope, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      dest_file = g_file_get_child (location, dest_base);
      dest_file_dir = g_file_get_parent (dest_file);

      if (!dex_await (dex_file_make_directory_with_parents (dest_file_dir), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      bytes = g_bytes_new_take (expand, strlen (expand)), expand = NULL;

      if (!dex_await (dex_file_replace_contents_bytes (dest_file, bytes, NULL, FALSE, 0), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_gobject_code_template_expand (FoundryTemplate *template)
{
  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              plugin_gobject_code_template_expand_fiber,
                              g_object_ref (template),
                              g_object_unref);
}

static void
plugin_gobject_code_template_finalize (GObject *object)
{
  PluginGobjectCodeTemplate *self = (PluginGobjectCodeTemplate *)object;

  g_clear_object (&self->input);

  G_OBJECT_CLASS (plugin_gobject_code_template_parent_class)->finalize (object);
}

static void
plugin_gobject_code_template_class_init (PluginGobjectCodeTemplateClass *klass)
{
  FoundryTemplateClass *template_class = FOUNDRY_TEMPLATE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_gobject_code_template_finalize;

  template_class->dup_id = plugin_gobject_code_template_dup_id;
  template_class->dup_description = plugin_gobject_code_template_dup_description;
  template_class->dup_input = plugin_gobject_code_template_dup_input;
  template_class->expand = plugin_gobject_code_template_expand;
}

static void
plugin_gobject_code_template_init (PluginGobjectCodeTemplate *self)
{
}

FoundryCodeTemplate *
plugin_gobject_code_template_new (const PluginGobjectCodeTemplateInfo *info,
                                  FoundryContext                      *context)
{
  PluginGobjectCodeTemplate *self;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (!context || FOUNDRY_IS_CONTEXT (context), NULL);

  self = g_object_new (PLUGIN_TYPE_GOBJECT_CODE_TEMPLATE,
                       "context", context,
                       NULL);
  self->info = info;

  return FOUNDRY_CODE_TEMPLATE (self);
}
