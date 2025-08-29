/* foundry-internal-tweak.c
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

#include "foundry-input-font.h"
#include "foundry-input-switch.h"
#include "foundry-internal-tweak.h"
#include "foundry-settings.h"
#include "foundry-tweak-info.h"
#include "foundry-tweak-info-private.h"

struct _FoundryInternalTweak
{
  FoundryTweak      parent_instance;
  char             *path;
  FoundryTweakInfo *info;
  const char       *gettext_domain;
  GSettings        *settings;
};

G_DEFINE_FINAL_TYPE (FoundryInternalTweak, foundry_internal_tweak, FOUNDRY_TYPE_TWEAK)

static char *
foundry_internal_tweak_dup_title (FoundryTweak *tweak)
{
  FoundryInternalTweak *self = FOUNDRY_INTERNAL_TWEAK (tweak);

  return g_strdup (g_dgettext (self->gettext_domain, self->info->title));
}

static char *
foundry_internal_tweak_dup_subtitle (FoundryTweak *tweak)
{
  FoundryInternalTweak *self = FOUNDRY_INTERNAL_TWEAK (tweak);

  return g_strdup (g_dgettext (self->gettext_domain, self->info->subtitle));
}

static char *
foundry_internal_tweak_dup_display_hint (FoundryTweak *tweak)
{
  FoundryInternalTweak *self = FOUNDRY_INTERNAL_TWEAK (tweak);

  if (self->info->display_hint)
    return g_strdup (self->info->display_hint);

  if (self->info->type == FOUNDRY_TWEAK_TYPE_GROUP)
    return g_strdup ("group");

  return NULL;
}

static char *
foundry_internal_tweak_dup_sort_key (FoundryTweak *tweak)
{
  return g_strdup (FOUNDRY_INTERNAL_TWEAK (tweak)->info->sort_key);
}

static char *
foundry_internal_tweak_dup_path (FoundryTweak *tweak)
{
  return g_strdup (FOUNDRY_INTERNAL_TWEAK (tweak)->path);
}

static char *
foundry_internal_tweak_dup_section (FoundryTweak *tweak)
{
  return g_strdup (FOUNDRY_INTERNAL_TWEAK (tweak)->info->section);
}

static GIcon *
foundry_internal_tweak_dup_icon (FoundryTweak *tweak)
{
  FoundryInternalTweak *self = FOUNDRY_INTERNAL_TWEAK (tweak);

  if (self->info->icon_name)
    {
      GIcon *icon = g_icon_new_for_string (self->info->icon_name, NULL);

      if (icon != NULL)
        return icon;

      return g_themed_icon_new (self->info->icon_name);
    }

  return NULL;
}

static GSettings *
create_settings (FoundryInternalTweak *self,
                 FoundryContext       *context,
                 const char           *schema_id,
                 const char           *path)
{
  g_autoptr(FoundrySettings) settings = NULL;

  g_assert (FOUNDRY_IS_CONTEXT (context));
  g_assert (schema_id != NULL);

  if (path != NULL)
    settings = foundry_settings_new_with_path (context, schema_id, path);
  else
    settings = foundry_settings_new (context, schema_id);

  if (g_str_has_prefix (self->path, "/app/"))
    return foundry_settings_dup_layer (settings, FOUNDRY_SETTINGS_LAYER_APPLICATION);

  if (g_str_has_prefix (self->path, "/project/"))
    return foundry_settings_dup_layer (settings, FOUNDRY_SETTINGS_LAYER_PROJECT);

  if (g_str_has_prefix (self->path, "/user/"))
    return foundry_settings_dup_layer (settings, FOUNDRY_SETTINGS_LAYER_USER);

  g_return_val_if_reached (NULL);
}

static FoundryInput *
create_switch (const FoundryTweakInfo *info,
               GSettings              *settings,
               const char             *key)
{
  FoundryInput *input;
  gboolean value;

  g_assert (info != NULL);
  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL);

  value = g_settings_get_boolean (settings, key);
  input = foundry_input_switch_new (info->title, info->subtitle, NULL, value);

  g_settings_bind (settings, key, input, "value", G_SETTINGS_BIND_DEFAULT);

  return input;
}

static FoundryInput *
create_font (const FoundryTweakInfo *info,
             GSettings              *settings,
             const char             *key)
{
  g_autofree char *value = NULL;
  FoundryInput *input;

  g_assert (info != NULL);
  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL);

  value = g_settings_get_string (settings, key);
  input = foundry_input_font_new (info->title, info->subtitle, NULL, value);

  g_settings_bind (settings, key, input, "value", G_SETTINGS_BIND_DEFAULT);

  return input;
}

static FoundryInput *
foundry_internal_tweak_create_input (FoundryTweak   *tweak,
                                     FoundryContext *context)
{
  FoundryInternalTweak *self = FOUNDRY_INTERNAL_TWEAK (tweak);

  g_assert (FOUNDRY_IS_INTERNAL_TWEAK (self));
  g_assert (FOUNDRY_IS_CONTEXT (context));
  g_assert (self->info != NULL);

  if (self->info->source->type == FOUNDRY_TWEAK_SOURCE_TYPE_CALLBACK)
    return self->info->source->callback.callback (self->info);

  if (self->info->source->type == FOUNDRY_TWEAK_SOURCE_TYPE_SETTING)
    {
      g_autoptr(GSettingsSchemaKey) key = NULL;
      g_autoptr(GSettingsSchema) schema = NULL;
      const GVariantType *value_type;
      const char *key_name;

      if (self->settings == NULL)
        self->settings = create_settings (self,
                                          context,
                                          self->info->source->setting.schema_id,
                                          self->info->source->setting.path);

      if (self->settings == NULL)
        return NULL;

      g_object_get (self->settings,
                    "settings-schema", &schema,
                    NULL);

      key_name = self->info->source->setting.key;

      if (!(key = g_settings_schema_get_key (schema, key_name)))
        return NULL;

      value_type = g_settings_schema_key_get_value_type (key);

      if (self->info->type == FOUNDRY_TWEAK_TYPE_SWITCH &&
          g_variant_type_equal (value_type, G_VARIANT_TYPE_BOOLEAN))
        return create_switch (self->info, self->settings, key_name);
      else if (self->info->type == FOUNDRY_TWEAK_TYPE_FONT &&
          g_variant_type_equal (value_type, G_VARIANT_TYPE_STRING))
        return create_font (self->info, self->settings, key_name);
    }

  return NULL;
}

static void
foundry_internal_tweak_finalize (GObject *object)
{
  FoundryInternalTweak *self = (FoundryInternalTweak *)object;

  g_clear_pointer (&self->info, foundry_tweak_info_unref);
  g_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS (foundry_internal_tweak_parent_class)->finalize (object);
}

static void
foundry_internal_tweak_class_init (FoundryInternalTweakClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryTweakClass *tweak_class = FOUNDRY_TWEAK_CLASS (klass);

  object_class->finalize = foundry_internal_tweak_finalize;

  tweak_class->dup_path = foundry_internal_tweak_dup_path;
  tweak_class->dup_title = foundry_internal_tweak_dup_title;
  tweak_class->dup_subtitle = foundry_internal_tweak_dup_subtitle;
  tweak_class->dup_display_hint = foundry_internal_tweak_dup_display_hint;
  tweak_class->dup_sort_key = foundry_internal_tweak_dup_sort_key;
  tweak_class->dup_section = foundry_internal_tweak_dup_section;
  tweak_class->dup_icon = foundry_internal_tweak_dup_icon;
  tweak_class->create_input = foundry_internal_tweak_create_input;
}

static void
foundry_internal_tweak_init (FoundryInternalTweak *self)
{
}

FoundryTweak *
foundry_internal_tweak_new (const char       *gettext_domain,
                            FoundryTweakInfo *info,
                            char             *path)
{
  FoundryInternalTweak *self;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_INTERNAL_TWEAK, NULL);
  self->gettext_domain = gettext_domain ? g_intern_string (gettext_domain) : GETTEXT_PACKAGE;
  self->info = info;
  self->path = path;

  return FOUNDRY_TWEAK (self);
}
