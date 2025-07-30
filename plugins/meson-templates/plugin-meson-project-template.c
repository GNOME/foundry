/* plugin-meson-project-template.c
 *
 * Copyright 2022-2025 Christian Hergert <chergert@redhat.com>
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

#include <string.h>

#include <glib/gi18n-lib.h>

#include <foundry.h>
#include <tmpl-glib.h>

#include "plugin-meson-project-template.h"

struct _PluginMesonProjectTemplate
{
  FoundryProjectTemplate         parent_instance;
  const PluginMesonTemplateInfo *info;
  FoundryInput                  *input;
};

G_DEFINE_FINAL_TYPE (PluginMesonProjectTemplate, plugin_meson_project_template, FOUNDRY_TYPE_PROJECT_TEMPLATE)

static GRegex *app_id_regex;
static GRegex *name_regex;

static void
add_to_scope (TmplScope  *scope,
              const char *pattern)
{
  g_autofree char *key = NULL;
  const char *val;

  g_assert (scope != NULL);
  g_assert (pattern != NULL);

  val = strchr (pattern, '=');

  /* If it is just "FOO" then set "FOO" to True */
  if (val == NULL)
    {
      tmpl_scope_set_boolean (scope, pattern, TRUE);
      return;
    }

  key = g_strndup (pattern, val - pattern);
  val++;

  /* If simple key=value, set the bool/string */
  if (strstr (val, "{{") == NULL)
    {
      if (foundry_str_equal0 (val, "false"))
        tmpl_scope_set_boolean (scope, key, FALSE);
      else if (foundry_str_equal0 (val, "true"))
        tmpl_scope_set_boolean (scope, key, TRUE);
      else
        tmpl_scope_set_string (scope, key, val);

      return;
    }

  /* More complex, we have a template to expand from scope */
  {
    g_autoptr(TmplTemplate) template = tmpl_template_new (NULL);
    g_autoptr(GError) error = NULL;
    g_autofree char *expanded = NULL;

    if (!tmpl_template_parse_string (template, val, &error))
      {
        g_warning ("Failed to parse template %s: %s",
                   val, error->message);
        return;
      }

    if (!(expanded = tmpl_template_expand_string (template, scope, &error)))
      {
        g_warning ("Failed to expand template %s: %s",
                   val, error->message);
        return;
      }

    tmpl_scope_set_string (scope, key, expanded);
  }
}

#if 0
static void
plugin_meson_project_template_expand_async (FoundryProjectTemplate *template,
                                            IdeTemplateInput       *input,
                                            TmplScope              *scope,
                                            GCancellable           *cancellable,
                                            GAsyncReadyCallback     callback,
                                            gpointer                user_data)
{
  PluginMesonProjectTemplate *self = (PluginMesonProjectTemplate *)template;
  g_autofree char *license_path = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) destdir = NULL;
  const char *language;
  const char *name;
  GFile *directory;

  FOUNDRY_ENTRY;

  g_assert (PLUGIN_IS_MESON_PROJECT_TEMPLATE (template));
  g_assert (FOUNDRY_IS_TEMPLATE_INPUT (input));
  g_assert (scope != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (template, cancellable, callback, user_data);
  ide_task_set_source_tag (task, plugin_meson_project_template_expand_async);

  name = ide_template_input_get_name (input);
  language = ide_template_input_get_language (input);
  directory = ide_template_input_get_directory (input);
  destdir = g_file_get_child (directory, name);

  if (self->expansions == NULL || self->n_expansions == 0)
    {
      ide_task_return_unsupported_error (task);
      FOUNDRY_EXIT;
    }

  /* Setup our license for the project */
  if ((license_path = ide_template_input_get_license_path (input)))
    {
      g_autoptr(GFile) copying = g_file_get_child (destdir, "COPYING");
      ide_template_base_add_resource (FOUNDRY_TEMPLATE_BASE (self),
                                      license_path, copying, scope, 0);
    }

  /* First setup some defaults for our scope */
  tmpl_scope_set_boolean (scope, "is_adwaita", FALSE);
  tmpl_scope_set_boolean (scope, "is_gtk4", FALSE);
  tmpl_scope_set_boolean (scope, "is_cli", FALSE);
  tmpl_scope_set_boolean (scope, "enable_gnome", FALSE);
  tmpl_scope_set_boolean (scope, "enable_i18n", FALSE);

  /* Add any extra scope to the expander which might be needed */
  if (self->extra_scope != NULL)
    {
      for (guint j = 0; self->extra_scope[j]; j++)
        add_to_scope (scope, self->extra_scope[j]);
    }

  /* Now add any per-language scope necessary */
  if (self->language_scope != NULL)
    {
      for (guint j = 0; j < self->n_language_scope; j++)
        {
          if (!foundry_str_equal0 (language, self->language_scope[j].language) ||
              self->language_scope[j].extra_scope == NULL)
            continue;

          for (guint k = 0; self->language_scope[j].extra_scope[k]; k++)
            add_to_scope (scope, self->language_scope[j].extra_scope[k]);
        }
    }

  for (guint i = 0; i < self->n_expansions; i++)
    {
      const char *src = self->expansions[i].input;
      const char *dest = self->expansions[i].output_pattern;
      g_autofree char *dest_eval = NULL;
      g_autofree char *resource_path = NULL;
      g_autoptr(GFile) dest_file = NULL;
      int mode = 0;

      if (self->expansions[i].languages != NULL &&
          !g_strv_contains (self->expansions[i].languages, language))
        continue;

      /* Expand the destination filename if necessary using a template */
      if (strstr (dest, "{{") != NULL)
        {
          g_autoptr(TmplTemplate) expander = tmpl_template_new (NULL);
          g_autoptr(GError) error = NULL;

          if (!tmpl_template_parse_string (expander, dest, &error))
            {
              ide_task_return_error (task, g_steal_pointer (&error));
              FOUNDRY_EXIT;
            }

          if (!(dest_eval = tmpl_template_expand_string (expander, scope, &error)))
            {
              ide_task_return_error (task, g_steal_pointer (&error));
              FOUNDRY_EXIT;
            }

          dest = dest_eval;
        }

      resource_path = g_strdup_printf ("/app/devsuite/foundry/plugins/meson-templates/resources/%s", src);
      dest_file = g_file_get_child (destdir, dest);

      if (self->expansions[i].executable)
        mode = 0750;

      ide_template_base_add_resource (FOUNDRY_TEMPLATE_BASE (self),
                                      resource_path, dest_file, scope, mode);
    }

  ide_template_base_expand_all_async (FOUNDRY_TEMPLATE_BASE (self),
                                      cancellable,
                                      plugin_meson_project_template_expand_cb,
                                      g_steal_pointer (&task));

  FOUNDRY_EXIT;
}
#endif

static char *
plugin_meson_project_template_dup_id (FoundryTemplate *template)
{
  PluginMesonProjectTemplate *self = PLUGIN_MESON_PROJECT_TEMPLATE (template);

  return g_strdup (self->info->id);
}

static char *
plugin_meson_project_template_dup_description (FoundryTemplate *template)
{
  PluginMesonProjectTemplate *self = PLUGIN_MESON_PROJECT_TEMPLATE (template);

  return g_strdup (self->info->description);
}

static FoundryInput *
create_license_combo (void)
{
  g_autoptr(GListStore) choices = g_list_store_new (G_TYPE_OBJECT);
  g_autoptr(GListModel) licenses = foundry_license_list_all ();
  guint n_licenses = g_list_model_get_n_items (licenses);

  for (guint i = 0; i < n_licenses; i++)
    {
      g_autoptr(FoundryLicense) license = g_list_model_get_item (licenses, i);
      g_autofree char *title = foundry_license_dup_id (license);
      g_autoptr(FoundryInput) choice = foundry_input_choice_new (title, NULL, G_OBJECT (license));

      g_list_store_append (choices, choice);
    }

  return foundry_input_combo_new (_("License"), NULL, G_LIST_MODEL (choices));
}

static FoundryInput *
plugin_meson_project_template_dup_input (FoundryTemplate *template)
{
  PluginMesonProjectTemplate *self = (PluginMesonProjectTemplate *)template;

  g_assert (PLUGIN_IS_MESON_PROJECT_TEMPLATE (self));

  if (self->input == NULL)
    {
      g_autoptr(GPtrArray) items = g_ptr_array_new_with_free_func (g_object_unref);

      g_ptr_array_add (items,
                       foundry_input_text_new (_("Project Name"),
                                               _("A unique name that is used for the project folder and other resources. The name should be in lower case without spaces and should not start with a number."),
                                               foundry_input_validator_regex_new (name_regex),
                                               NULL));

      if (strstr (self->info->id, "gtk") ||
          strstr (self->info->id, "adwaita"))
        g_ptr_array_add (items,
                         foundry_input_text_new (_("Application ID"),
                                                 _("A reverse domain-name identifier used to identify the application, such as “org.gnome.Builder”. It may not contain dashes."),
                                                 foundry_input_validator_regex_new (app_id_regex),
                                                 NULL));

      g_ptr_array_add (items, create_license_combo ());
      g_ptr_array_add (items,
                       foundry_input_switch_new (_("Use Version Control"), NULL, TRUE));
      g_ptr_array_add (items,
                       foundry_input_text_new (_("Author Name"), NULL, NULL, g_get_real_name ()));
      g_ptr_array_add (items,
                       foundry_input_text_new (_("Author Email"), NULL, NULL, NULL));

      if (items->len > 0)
        self->input = foundry_input_group_new (self->info->name,
                                               self->info->description,
                                               (FoundryInput **)items->pdata,
                                               items->len);
    }

  return self->input ? g_object_ref (self->input) : NULL;
}

static void
plugin_meson_project_template_finalize (GObject *object)
{
  PluginMesonProjectTemplate *self = (PluginMesonProjectTemplate *)object;

  self->info = NULL;

  g_clear_object (&self->input);

  G_OBJECT_CLASS (plugin_meson_project_template_parent_class)->finalize (object);
}

static void
plugin_meson_project_template_class_init (PluginMesonProjectTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryTemplateClass *template_class = FOUNDRY_TEMPLATE_CLASS (klass);
  g_autoptr(GError) error = NULL;

  object_class->finalize = plugin_meson_project_template_finalize;

  template_class->dup_id = plugin_meson_project_template_dup_id;
  template_class->dup_description = plugin_meson_project_template_dup_description;
  template_class->dup_input = plugin_meson_project_template_dup_input;

  if (!(app_id_regex = g_regex_new ("^[A-Za-z][A-Za-z0-9_]*(\\.[A-Za-z][A-Za-z0-9_]*)+$", G_REGEX_ANCHORED, 0, &error)))
    g_error ("%s", error->message);

  if (!(name_regex = g_regex_new ("^[\x21-\x7E]+$", G_REGEX_ANCHORED, 0, &error)))
    g_error ("%s", error->message);
}

static void
plugin_meson_project_template_init (PluginMesonProjectTemplate *self)
{
}

FoundryProjectTemplate *
plugin_meson_project_template_new (const PluginMesonTemplateInfo *info)
{
  PluginMesonProjectTemplate *self;

  g_return_val_if_fail (info != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_MESON_PROJECT_TEMPLATE, NULL);
  self->info = info;

  return FOUNDRY_PROJECT_TEMPLATE (self);
}
