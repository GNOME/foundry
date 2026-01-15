/* foundry-forge-project.c
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

#include "foundry-forge-project.h"
#include "foundry-forge-query.h"
#include "foundry-util.h"

/**
 * FoundryForgeProject:
 *
 * Represents a project on a forge.
 *
 * Plugins can implement this as part of their forge implementation to allow
 * the user to access various forge features related to their project.
 *
 * Since: 1.1
 */

enum {
  PROP_0,
  PROP_AVATAR_URL,
  PROP_DESCRIPTION,
  PROP_ISSUES_URL,
  PROP_ONLINE_URL,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryForgeProject, foundry_forge_project, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_forge_project_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryForgeProject *self = FOUNDRY_FORGE_PROJECT (object);

  switch (prop_id)
    {
    case PROP_AVATAR_URL:
      g_value_take_string (value, foundry_forge_project_dup_avatar_url (self));
      break;

    case PROP_DESCRIPTION:
      g_value_take_string (value, foundry_forge_project_dup_description (self));
      break;

    case PROP_ISSUES_URL:
      g_value_take_string (value, foundry_forge_project_dup_issues_url (self));
      break;

    case PROP_ONLINE_URL:
      g_value_take_string (value, foundry_forge_project_dup_online_url (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_forge_project_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_project_class_init (FoundryForgeProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_forge_project_get_property;

  properties[PROP_AVATAR_URL] =
    g_param_spec_string ("avatar-url", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ISSUES_URL] =
    g_param_spec_string ("issues-url", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ONLINE_URL] =
    g_param_spec_string ("online-url", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_forge_project_init (FoundryForgeProject *self)
{
}

/**
 * foundry_forge_project_dup_issues_url:
 * @self: a [class@Foundry.ForgeProject]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_project_dup_issues_url (FoundryForgeProject *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_PROJECT (self), NULL);

  if (FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_issues_url)
    return FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_issues_url (self);

  return NULL;
}

/**
 * foundry_forge_project_dup_online_url:
 * @self: a [class@Foundry.ForgeProject]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_project_dup_online_url (FoundryForgeProject *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_PROJECT (self), NULL);

  if (FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_online_url)
    return FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_online_url (self);

  return NULL;
}

/**
 * foundry_forge_project_dup_avatar_url:
 * @self: a [class@Foundry.ForgeProject]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_project_dup_avatar_url (FoundryForgeProject *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_PROJECT (self), NULL);

  if (FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_avatar_url)
    return FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_avatar_url (self);

  return NULL;
}

/**
 * foundry_forge_project_dup_description:
 * @self: a [class@Foundry.ForgeProject]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_project_dup_description (FoundryForgeProject *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_PROJECT (self), NULL);

  if (FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_description)
    return FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_description (self);

  return NULL;
}

/**
 * foundry_forge_project_dup_title:
 * @self: a [class@Foundry.ForgeProject]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_project_dup_title (FoundryForgeProject *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_PROJECT (self), NULL);

  if (FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_title)
    return FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->dup_title (self);

  return NULL;
}

/**
 * foundry_forge_project_load_avatar:
 * @self: a [class@Foundry.ForgeProject]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [struct@GLib.Bytes] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_forge_project_load_avatar (FoundryForgeProject *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE_PROJECT (self));

  if (FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->load_avatar)
    return FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->load_avatar (self);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_forge_project_list_issues:
 * @self: a [class@Foundry.ForgeProject]
 * @query: (nullable):
 *
 * Queries the forge for a list of issues in the project.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.ForgeListing] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_forge_project_list_issues (FoundryForgeProject *self,
                                   FoundryForgeQuery   *query)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE_PROJECT (self));
  dex_return_error_if_fail (!query || FOUNDRY_IS_FORGE_QUERY (query));

  if (FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->list_issues)
    return FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->list_issues (self, query);

  return foundry_future_new_not_supported ();
}

/**
 * foundry_forge_project_list_merge_requests:
 * @self: a [class@Foundry.ForgeProject]
 * @query: (nullable):
 *
 * Queries the forge for a list of merge requests in the project.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.ForgeListing] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_forge_project_list_merge_requests (FoundryForgeProject *self,
                                           FoundryForgeQuery   *query)
{
  dex_return_error_if_fail (FOUNDRY_IS_FORGE_PROJECT (self));
  dex_return_error_if_fail (!query || FOUNDRY_IS_FORGE_QUERY (query));

  if (FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->list_merge_requests)
    return FOUNDRY_FORGE_PROJECT_GET_CLASS (self)->list_merge_requests (self, query);

  return foundry_future_new_not_supported ();
}
