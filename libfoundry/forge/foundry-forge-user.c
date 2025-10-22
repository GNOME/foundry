/* foundry-forge-user.c
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

#include "foundry-forge-user.h"
#include "foundry-util.h"

enum {
  PROP_0,
  PROP_BIO,
  PROP_HANDLE,
  PROP_LOCATION,
  PROP_ONLINE_URL,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryForgeUser, foundry_forge_user, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_forge_user_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryForgeUser *self = FOUNDRY_FORGE_USER (object);

  switch (prop_id)
    {
    case PROP_BIO:
      g_value_take_string (value, foundry_forge_user_dup_bio (self));
      break;

    case PROP_HANDLE:
      g_value_take_string (value, foundry_forge_user_dup_handle (self));
      break;

    case PROP_LOCATION:
      g_value_take_string (value, foundry_forge_user_dup_location (self));
      break;

    case PROP_ONLINE_URL:
      g_value_take_string (value, foundry_forge_user_dup_online_url (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_forge_user_dup_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_user_class_init (FoundryForgeUserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_forge_user_get_property;

  properties[PROP_BIO] =
    g_param_spec_string ("bio", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_HANDLE] =
    g_param_spec_string ("handle", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LOCATION] =
    g_param_spec_string ("location", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ONLINE_URL] =
    g_param_spec_string ("online-url", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_forge_user_init (FoundryForgeUser *self)
{
}

/**
 * foundry_forge_user_dup_handle:
 * @self: a [class@Foundry.ForgeUser]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_user_dup_handle (FoundryForgeUser *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_USER (self), NULL);

  if (FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_handle)
    return FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_handle (self);

  return NULL;
}

/**
 * foundry_forge_user_dup_name:
 * @self: a [class@Foundry.ForgeUser]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_user_dup_name (FoundryForgeUser *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_USER (self), NULL);

  if (FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_name)
    return FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_name (self);

  return NULL;
}

/**
 * foundry_forge_user_dup_location:
 * @self: a [class@Foundry.ForgeUser]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_user_dup_location (FoundryForgeUser *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_USER (self), NULL);

  if (FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_location)
    return FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_location (self);

  return NULL;
}

/**
 * foundry_forge_user_dup_bio:
 * @self: a [class@Foundry.ForgeUser]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_user_dup_bio (FoundryForgeUser *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_USER (self), NULL);

  if (FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_bio)
    return FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_bio (self);

  return NULL;
}

/**
 * foundry_forge_user_dup_online_url:
 * @self: a [class@Foundry.ForgeUser]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_user_dup_online_url (FoundryForgeUser *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_USER (self), NULL);

  if (FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_online_url)
    return FOUNDRY_FORGE_USER_GET_CLASS (self)->dup_online_url (self);

  return NULL;
}

/**
 * foundry_forge_user_load_avatar:
 * @self: a [class@Foundry.ForgeUser]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [struct@GLib.Bytes] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_forge_user_load_avatar (FoundryForgeUser *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE_USER (self));

  if (FOUNDRY_FORGE_USER_GET_CLASS (self)->load_avatar)
    return FOUNDRY_FORGE_USER_GET_CLASS (self)->load_avatar (self);

  return foundry_future_new_not_supported ();
}
