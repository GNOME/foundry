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

#include "foundry-context-private.h"

#include "plugin-meson-project-template.h"
#include "plugin-meson-template-locator.h"

struct _PluginMesonProjectTemplate
{
  FoundryProjectTemplate         parent_instance;

  const PluginMesonTemplateInfo *info;

  FoundryInput                  *input;
  FoundryInput                  *app_id;
  FoundryInput                  *author_email;
  FoundryInput                  *author_name;
  FoundryInput                  *language;
  FoundryInput                  *license;
  FoundryInput                  *location;
  FoundryInput                  *project_name;
  FoundryInput                  *project_version;
  FoundryInput                  *version_control;
};

G_DEFINE_FINAL_TYPE (PluginMesonProjectTemplate, plugin_meson_project_template, FOUNDRY_TYPE_PROJECT_TEMPLATE)

static GRegex *app_id_regex;
static GRegex *name_regex;

static void
scope_take_string (TmplScope  *scope,
                   const char *name,
                   char       *value)
{
  tmpl_scope_set_string (scope, name, value);
  g_free (value);
}

static char *
capitalize (const gchar *input)
{
  gunichar c;
  GString *str;

  if (input == NULL)
    return NULL;

  if (*input == 0)
    return g_strdup ("");

  c = g_utf8_get_char (input);
  if (g_unichar_isupper (c))
    return g_strdup (input);

  str = g_string_new (NULL);
  input = g_utf8_next_char (input);
  g_string_append_unichar (str, g_unichar_toupper (c));
  if (*input)
    g_string_append (str, input);

  return g_string_free (str, FALSE);
}

static char *
camelize (const char *input)
{
  gboolean next_is_upper = TRUE;
  gboolean skip = FALSE;
  GString *str;

  if (input == NULL)
    return NULL;

  if (!strchr (input, '_') && !strchr (input, ' ') && !strchr (input, '-'))
    return capitalize (input);

  str = g_string_new (NULL);

	for (; *input; input = g_utf8_next_char (input))
    {
      gunichar c = g_utf8_get_char (input);

      switch (c)
      {
      case '_':
      case '-':
      case ' ':
        next_is_upper = TRUE;
        skip = TRUE;
        break;

      default:
        break;
      }

      if (skip)
        {
          skip = FALSE;
          continue;
        }

      if (next_is_upper)
        {
          c = g_unichar_toupper (c);
          next_is_upper = FALSE;
        }
      else
        c = g_unichar_tolower (c);

      g_string_append_unichar (str, c);
    }

  if (g_str_has_suffix (str->str, "Private"))
    g_string_truncate (str, str->len - strlen ("Private"));

  return g_string_free (str, FALSE);
}

static char *
functify (const gchar *input)
{
  gunichar last = 0;
  GString *str;

  if (input == NULL)
    return NULL;

  str = g_string_new (NULL);

  for (; *input; input = g_utf8_next_char (input))
    {
      gunichar c = g_utf8_get_char (input);
      gunichar n = g_utf8_get_char (g_utf8_next_char (input));

      if (last)
        {
          if ((g_unichar_islower (last) && g_unichar_isupper (c)) ||
              (g_unichar_isupper (c) && g_unichar_islower (n)))
            g_string_append_c (str, '_');
        }

      if ((c == ' ') || (c == '-'))
        c = '_';

      g_string_append_unichar (str, g_unichar_tolower (c));

      last = c;
    }

  if (g_str_has_suffix (str->str, "_private") ||
      g_str_has_suffix (str->str, "_PRIVATE"))
    g_string_truncate (str, str->len - strlen ("_private"));

  return g_string_free (str, FALSE);
}

static char *
build_app_path (const char *app_id)
{
  GString *str = g_string_new ("/");

  for (const char *c = app_id; *c; c = g_utf8_next_char (c))
    {
      if (*c == '.')
        g_string_append_c (str, '/');
      else
        g_string_append_unichar (str, g_utf8_get_char (c));
    }

  return g_string_free (str, FALSE);
}

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

static void
setup_basic_scope (PluginMesonProjectTemplate *self,
                   TmplScope                  *scope,
                   TmplTemplateLocator        *locator)
{
  g_autoptr(FoundryInputChoice) license_choice = NULL;
  g_autoptr(FoundryInputChoice) language = NULL;
  g_autoptr(FoundryLicense) license = NULL;
  g_autoptr(GDateTime) now = NULL;
  g_autoptr(GString) author_escape = NULL;
  g_autoptr(GFile) dir = NULL;
  g_autofree char *author_name = NULL;
  g_autofree char *project_name = NULL;
  g_autofree char *name = NULL;
  g_autofree char *name_lower = NULL;
  g_autofree char *prefix_ = NULL;
  g_autofree char *prefix = NULL;
  g_autofree char *Prefix = NULL;
  g_autofree char *PreFix = NULL;
  g_autofree char *app_id = NULL;
  g_autofree char *language_name = NULL;

  g_assert (PLUGIN_IS_MESON_PROJECT_TEMPLATE (self));
  g_assert (scope != NULL);

  now = g_date_time_new_now_local ();

  dir = foundry_input_file_dup_value (FOUNDRY_INPUT_FILE (self->location));
  author_name = foundry_input_text_dup_value (FOUNDRY_INPUT_TEXT (self->author_name));
  author_escape = g_string_new (author_name);
  g_string_replace (author_escape, "'", "\\'", 0);

  name = foundry_input_text_dup_value (FOUNDRY_INPUT_TEXT (self->project_name));
  app_id = foundry_input_text_dup_value (FOUNDRY_INPUT_TEXT (self->app_id));
  language = foundry_input_combo_dup_choice (FOUNDRY_INPUT_COMBO (self->language));
  language_name = foundry_input_dup_title (FOUNDRY_INPUT (language));

  if (foundry_str_empty0 (app_id))
    g_set_str (&app_id, "com.example.App");

  tmpl_scope_set_string (scope, "appid", app_id);
  scope_take_string (scope, "appid_path", build_app_path (app_id));

  tmpl_scope_set_string (scope, "template", self->info->name);
  scope_take_string (scope, "author", foundry_input_text_dup_value (FOUNDRY_INPUT_TEXT (self->author_name)));
  tmpl_scope_set_string (scope, "author_escape", author_escape->str);
  scope_take_string (scope, "project_version", foundry_input_text_dup_value (FOUNDRY_INPUT_TEXT (self->project_version)));
  scope_take_string (scope, "language", g_utf8_strdown (language_name, -1));
  tmpl_scope_set_boolean (scope, "versioning", foundry_input_switch_get_value (FOUNDRY_INPUT_SWITCH (self->version_control)));
  scope_take_string (scope, "project_path", g_file_get_path (dir));

  /* Name variants for use as classes, functions, etc */
  name_lower = g_utf8_strdown (name ? name : "example", -1);
  tmpl_scope_set_string (scope, "name", name_lower);
  scope_take_string (scope, "name_", functify (name_lower));
  scope_take_string (scope, "NAME", g_strdelimit (g_utf8_strup (name_lower, -1), "-", '_'));
  scope_take_string (scope, "year", g_date_time_format (now, "%Y"));
  scope_take_string (scope, "YEAR", g_date_time_format (now, "%Y"));
  scope_take_string (scope, "Title", capitalize (name));

  if (g_str_has_suffix (name_lower, "_glib"))
    prefix = g_strndup (name_lower, strlen (name_lower) - 5);
  else
    prefix = g_strdup (name_lower);
  Prefix = capitalize (prefix);
  PreFix = camelize (prefix);
  prefix_ = g_strdelimit (g_utf8_strdown (prefix, -1), "-", '_');

  /* Various prefixes for use as namespaces, etc */
  tmpl_scope_set_string (scope, "prefix", prefix);
  tmpl_scope_set_string (scope, "prefix_", prefix_);
  scope_take_string (scope, "PREFIX", g_strdelimit (g_utf8_strup (prefix, -1), "-", '_'));
  tmpl_scope_set_string (scope, "Prefix", Prefix);
  tmpl_scope_set_string (scope, "PreFix", PreFix);
  scope_take_string (scope, "spaces", g_strnfill (strlen (prefix_), ' '));
  scope_take_string (scope, "Spaces", g_strnfill (strlen (PreFix), ' '));

  if ((license_choice = foundry_input_combo_dup_choice (FOUNDRY_INPUT_COMBO (self->license))) &&
      (license = FOUNDRY_LICENSE (foundry_input_choice_dup_item (license_choice))))
    {
      g_autoptr(GBytes) license_text = foundry_license_dup_snippet_text (license);

      scope_take_string (scope, "project_license", foundry_license_dup_id (license));
      plugin_meson_template_locator_set_license_text (PLUGIN_MESON_TEMPLATE_LOCATOR (locator), license_text);
    }
  else
    {
      tmpl_scope_set_string (scope, "project_license", "");
    }
}

static gboolean
write_license (PluginMesonProjectTemplate  *self,
               GFile                       *dest_dir,
               GError                     **error)
{
  g_autoptr(FoundryInputChoice) choice = NULL;
  g_autoptr(FoundryLicense) license = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GFile) copying = NULL;

  g_assert (PLUGIN_IS_MESON_PROJECT_TEMPLATE (self));

  if ((choice = foundry_input_combo_dup_choice (FOUNDRY_INPUT_COMBO (self->license))) &&
      (license = FOUNDRY_LICENSE (foundry_input_choice_dup_item (choice))) &&
      (bytes = foundry_license_dup_text (license)) &&
      (copying = g_file_get_child (dest_dir, "LICENSE")))
    return dex_await (dex_file_replace_contents_bytes (copying, bytes, NULL, FALSE, 0), error);

  return TRUE;
}

static DexFuture *
plugin_meson_project_template_expand_fiber (gpointer user_data)
{
  PluginMesonProjectTemplate *self = user_data;
  const PluginMesonTemplateInfo *info;
  g_autoptr(TmplTemplateLocator) locator = NULL;
  g_autoptr(FoundryInput) input = NULL;
  g_autoptr(TmplScope) scope = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) directory = NULL;
  g_autoptr(GFile) destdir = NULL;
  g_autofree char *project_name = NULL;
  g_autofree char *language = NULL;

  g_assert (PLUGIN_IS_MESON_PROJECT_TEMPLATE (self));

  info = self->info;

  input = foundry_template_dup_input (FOUNDRY_TEMPLATE (self));

  if (!dex_await (foundry_input_validate (input), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  locator = plugin_meson_template_locator_new ();
  tmpl_template_locator_append_search_path (locator,
                                            "resource:///app/devsuite/foundry/plugins/meson-templates/resources/");

  scope = tmpl_scope_new ();

  tmpl_scope_set_boolean (scope, "is_adwaita", FALSE);
  tmpl_scope_set_boolean (scope, "is_gtk4", FALSE);
  tmpl_scope_set_boolean (scope, "is_cli", FALSE);
  tmpl_scope_set_boolean (scope, "enable_gnome", FALSE);
  tmpl_scope_set_boolean (scope, "enable_i18n", FALSE);

  setup_basic_scope (self, scope, locator);

  project_name = foundry_input_text_dup_value (FOUNDRY_INPUT_TEXT (self->project_name));
  directory = foundry_input_file_dup_value (FOUNDRY_INPUT_FILE (self->location));
  destdir = g_file_get_child (directory, project_name);

  if (!dex_await (dex_file_make_directory_with_parents (destdir), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!write_license (self, destdir, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (info->extra_scope != NULL)
    {
      for (guint j = 0; info->extra_scope[j]; j++)
        add_to_scope (scope, info->extra_scope[j]);
    }

  if (info->language_scope != NULL)
    {
      g_autoptr(FoundryInputChoice) choice = foundry_input_combo_dup_choice (FOUNDRY_INPUT_COMBO (self->language));

      language = foundry_input_dup_title (FOUNDRY_INPUT (choice));

      for (guint j = 0; j < info->n_language_scope; j++)
        {
          if (!foundry_str_equal0 (language, info->language_scope[j].language) ||
              info->language_scope[j].extra_scope == NULL)
            continue;

          for (guint k = 0; info->language_scope[j].extra_scope[k]; k++)
            add_to_scope (scope, info->language_scope[j].extra_scope[k]);
        }
    }

  for (guint i = 0; i < info->n_expansions; i++)
    {
      const char *src = info->expansions[i].input;
      const char *dest = info->expansions[i].output_pattern;
      g_autoptr(TmplTemplate) template = NULL;
      g_autoptr(GBytes) bytes = NULL;
      g_autoptr(GFile) dest_file = NULL;
      g_autoptr(GFile) dest_file_dir = NULL;
      g_autofree char *dest_eval = NULL;
      g_autofree char *resource_path = NULL;
      g_autofree char *expand = NULL;
      g_autofree char *filename = NULL;

      if (info->expansions[i].languages != NULL &&
          !g_strv_contains (info->expansions[i].languages, language))
        continue;

      /* Expand the destination filename if necessary using a template */
      if (strstr (dest, "{{") != NULL)
        {
          g_autoptr(TmplTemplate) expander = tmpl_template_new (NULL);

          if (!tmpl_template_parse_string (expander, dest, &error))
            return dex_future_new_for_error (g_steal_pointer (&error));

          if (!(dest_eval = tmpl_template_expand_string (expander, scope, &error)))
            return dex_future_new_for_error (g_steal_pointer (&error));

          dest = dest_eval;
        }

      template = tmpl_template_new (locator);
      resource_path = g_strdup_printf ("/app/devsuite/foundry/plugins/meson-templates/resources/%s", src);

      dest_file = g_file_get_child (destdir, dest);
      dest_file_dir = g_file_get_parent (dest_file);
      filename = g_file_get_basename (dest_file);

      tmpl_scope_set_string (scope, "filename", filename);

      if (!tmpl_template_parse_resource (template, resource_path, NULL, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(expand = tmpl_template_expand_string (template, scope, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!dex_await (dex_file_make_directory_with_parents (dest_file_dir), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      bytes = g_bytes_new_take (expand, strlen (expand)), expand = NULL;

      if (!dex_await (dex_file_replace_contents_bytes (dest_file, bytes, NULL, FALSE, 0), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (info->expansions[i].executable)
        {
          g_autoptr(GFileInfo) file_info = g_file_info_new ();

          g_file_info_set_attribute_uint32 (file_info,
                                            G_FILE_ATTRIBUTE_UNIX_MODE,
                                            0750);

          if (!dex_await (dex_file_set_attributes (dest_file, file_info, 0, 0), &error))
            return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

  if (!dex_await (_foundry_context_initialize (destdir), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

#ifdef FOUNDRY_FEATURE_GIT
  if (foundry_input_switch_get_value (FOUNDRY_INPUT_SWITCH (self->version_control)))
    {
      if (!dex_await (foundry_git_initialize (destdir, FALSE), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }
#endif

  return dex_future_new_true ();
}

static DexFuture *
plugin_meson_project_template_expand (FoundryTemplate *template)
{
  g_assert (PLUGIN_IS_MESON_PROJECT_TEMPLATE (template));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_meson_project_template_expand_fiber,
                              g_object_ref (template),
                              g_object_unref);
}

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

  return foundry_input_combo_new (_("License"), NULL, NULL, G_LIST_MODEL (choices));
}

static FoundryInput *
create_language_combo (const char * const *languages)
{
  g_autoptr(GListStore) choices = g_list_store_new (G_TYPE_OBJECT);

  for (guint i = 0; languages[i]; i++)
    {
      const char *title = languages[i];
      g_autoptr(FoundryInput) choice = foundry_input_choice_new (title, NULL, NULL);

      g_list_store_append (choices, choice);
    }

  return foundry_input_combo_new (_("Language"), NULL, NULL, G_LIST_MODEL (choices));
}

static FoundryInput *
plugin_meson_project_template_dup_input (FoundryTemplate *template)
{
  PluginMesonProjectTemplate *self = (PluginMesonProjectTemplate *)template;

  g_assert (PLUGIN_IS_MESON_PROJECT_TEMPLATE (self));

  if (self->input == NULL)
    {
      g_autoptr(GPtrArray) items = g_ptr_array_new ();
      g_autoptr(GFile) default_location = foundry_dup_projects_directory_file ();

      self->app_id = foundry_input_text_new (_("Application ID"),
                                             _("A reverse domain-name identifier used to identify the application, such as “org.gnome.Builder”. It may not contain dashes."),
                                             foundry_input_validator_regex_new (app_id_regex),
                                             NULL);
      self->project_name = foundry_input_text_new (_("Project Name"),
                                                   _("A unique name that is used for the project folder and other resources. The name should be in lower case without spaces and should not start with a number."),
                                                   foundry_input_validator_regex_new (name_regex),
                                                   NULL);
      self->project_version = foundry_input_text_new (_("Project Version"),
                                                      _("The initial version number for the project"),
                                                      NULL, "0.1");
      self->location = foundry_input_file_new (_("Location"),
                                               NULL,
                                               NULL,
                                               G_FILE_TYPE_DIRECTORY,
                                               default_location);
      self->license = create_license_combo ();
      self->language = create_language_combo (self->info->languages);
      self->version_control = foundry_input_switch_new (_("Use Version Control"), NULL, NULL, TRUE);
      self->author_name = foundry_input_text_new (_("Author Name"), NULL, NULL, g_get_real_name ());
      self->author_email = foundry_input_text_new (_("Author Email"), NULL, NULL, NULL);

      g_ptr_array_add (items, self->project_name);

      if (strstr (self->info->id, "gtk") ||
          strstr (self->info->id, "adwaita"))
        g_ptr_array_add (items, self->app_id);

      g_ptr_array_add (items, self->location);
      g_ptr_array_add (items, self->language);
      g_ptr_array_add (items, self->license);
      g_ptr_array_add (items, self->version_control);

      if (items->len > 0)
        self->input = foundry_input_group_new (self->info->name,
                                               self->info->description,
                                               NULL,
                                               (FoundryInput **)items->pdata,
                                               items->len);
    }

  return self->input ? g_object_ref (self->input) : NULL;
}

static char **
plugin_meson_project_template_dup_tags (FoundryTemplate *template)
{
  return g_strdupv ((char **)PLUGIN_MESON_PROJECT_TEMPLATE (template)->info->tags);
}

static void
plugin_meson_project_template_finalize (GObject *object)
{
  PluginMesonProjectTemplate *self = (PluginMesonProjectTemplate *)object;

  self->info = NULL;

  g_clear_object (&self->input);
  g_clear_object (&self->app_id);
  g_clear_object (&self->author_email);
  g_clear_object (&self->author_name);
  g_clear_object (&self->language);
  g_clear_object (&self->license);
  g_clear_object (&self->location);
  g_clear_object (&self->project_name);
  g_clear_object (&self->project_version);
  g_clear_object (&self->version_control);

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
  template_class->dup_tags = plugin_meson_project_template_dup_tags;
  template_class->expand = plugin_meson_project_template_expand;

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
