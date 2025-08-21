/* foundry-tweak.c
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

#include "foundry-tweak.h"

typedef struct
{
  GIcon *icon;
  char  *display_hint;
  char  *sort_key;
  char  *subtitle;
  char  *title;
} FoundryTweakPrivate;

enum {
  PROP_0,
  PROP_DISPLAY_HINT,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_SORT_KEY,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryTweak, foundry_tweak, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_tweak_finalize (GObject *object)
{
  FoundryTweak *self = (FoundryTweak *)object;
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_clear_object (&priv->icon);

  g_clear_pointer (&priv->display_hint, g_free);
  g_clear_pointer (&priv->sort_key, g_free);
  g_clear_pointer (&priv->subtitle, g_free);
  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (foundry_tweak_parent_class)->finalize (object);
}

static void
foundry_tweak_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  FoundryTweak *self = FOUNDRY_TWEAK (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_HINT:
      g_value_take_string (value, foundry_tweak_dup_display_hint (self));
      break;

    case PROP_ICON:
      g_value_take_object (value, foundry_tweak_dup_icon (self));
      break;

    case PROP_SORT_KEY:
      g_value_take_string (value, foundry_tweak_dup_sort_key (self));
      break;

    case PROP_SUBTITLE:
      g_value_take_string (value, foundry_tweak_dup_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_tweak_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_tweak_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  FoundryTweak *self = FOUNDRY_TWEAK (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_HINT:
      foundry_tweak_set_display_hint (self, g_value_get_string (value));
      break;

    case PROP_ICON:
      foundry_tweak_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      foundry_tweak_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_SORT_KEY:
      foundry_tweak_set_sort_key (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      foundry_tweak_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      foundry_tweak_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_tweak_class_init (FoundryTweakClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_tweak_finalize;
  object_class->get_property = foundry_tweak_get_property;
  object_class->set_property = foundry_tweak_set_property;

  properties[PROP_DISPLAY_HINT] =
    g_param_spec_string ("display-hint", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_WRITABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SORT_KEY] =
    g_param_spec_string ("sort-key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_tweak_init (FoundryTweak *self)
{
}

char *
foundry_tweak_dup_display_hint (FoundryTweak *self)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_TWEAK (self), NULL);

  return g_strdup (priv->display_hint);
}

char *
foundry_tweak_dup_sort_key (FoundryTweak *self)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_TWEAK (self), NULL);

  return g_strdup (priv->sort_key);
}

char *
foundry_tweak_dup_subtitle (FoundryTweak *self)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_TWEAK (self), NULL);

  return g_strdup (priv->subtitle);
}

char *
foundry_tweak_dup_title (FoundryTweak *self)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_TWEAK (self), NULL);

  return g_strdup (priv->title);
}

/**
 * foundry_tweak_dup_icon:
 * @self: a [class@Foundry.Tweak]
 *
 * Returns: (transfer full) (nullable):
 */
GIcon *
foundry_tweak_dup_icon (FoundryTweak *self)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_TWEAK (self), NULL);

  return priv->icon ? g_object_ref (priv->icon) : NULL;
}

void
foundry_tweak_set_display_hint (FoundryTweak *self,
                                const char   *display_hint)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_TWEAK (self));

  if (g_set_str (&priv->display_hint, display_hint))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DISPLAY_HINT]);
}

void
foundry_tweak_set_sort_key (FoundryTweak *self,
                            const char   *sort_key)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_TWEAK (self));

  if (g_set_str (&priv->sort_key, sort_key))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORT_KEY]);
}

void
foundry_tweak_set_subtitle (FoundryTweak *self,
                            const char   *subtitle)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_TWEAK (self));

  if (g_set_str (&priv->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUBTITLE]);
}

void
foundry_tweak_set_title (FoundryTweak *self,
                         const char   *title)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_TWEAK (self));

  if (g_set_str (&priv->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

void
foundry_tweak_set_icon (FoundryTweak *self,
                        GIcon        *icon)
{
  FoundryTweakPrivate *priv = foundry_tweak_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_TWEAK (self));

  if (g_set_object (&priv->icon, icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
}

void
foundry_tweak_set_icon_name (FoundryTweak *self,
                             const char   *icon_name)
{
  g_autoptr(GIcon) icon = NULL;

  g_return_if_fail (FOUNDRY_IS_TWEAK (self));

  if (icon_name != NULL)
    icon = g_themed_icon_new (icon_name);

  foundry_tweak_set_icon (self, icon);
}
