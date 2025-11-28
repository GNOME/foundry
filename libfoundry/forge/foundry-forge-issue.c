/* foundry-forge-issue.c
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

#include "foundry-forge-issue.h"
#include "foundry-forge-user.h"

/**
 * FoundryForgeIssue:
 *
 * Abstract base class for representing issues from forge services.
 *
 * FoundryForgeIssue provides the core interface for representing issues
 * and bug reports from forge services. It includes common properties
 * like ID, title, state, and creation date, and provides a unified
 * interface for issue management across different forge platforms.
 */

enum {
  PROP_0,
  PROP_AUTHOR,
  PROP_CREATED_AT,
  PROP_DESCRIPTION,
  PROP_ID,
  PROP_ONLINE_URL,
  PROP_STATE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryForgeIssue, foundry_forge_issue, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_forge_issue_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryForgeIssue *self = FOUNDRY_FORGE_ISSUE (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, foundry_forge_issue_dup_id (self));
      break;

    case PROP_DESCRIPTION:
      g_value_take_string (value, foundry_forge_issue_dup_description (self));
      break;

    case PROP_STATE:
      g_value_take_string (value, foundry_forge_issue_dup_state (self));
      break;

    case PROP_ONLINE_URL:
      g_value_take_string (value, foundry_forge_issue_dup_online_url (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, foundry_forge_issue_dup_title (self));
      break;

    case PROP_CREATED_AT:
      g_value_take_boxed (value, foundry_forge_issue_dup_created_at (self));
      break;

    case PROP_AUTHOR:
      g_value_take_object (value, foundry_forge_issue_dup_author (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_issue_class_init (FoundryForgeIssueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_forge_issue_get_property;

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ONLINE_URL] =
    g_param_spec_string ("online-url", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_STATE] =
    g_param_spec_string ("state", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_CREATED_AT] =
    g_param_spec_boxed ("created-at", NULL, NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_AUTHOR] =
    g_param_spec_object ("author", NULL, NULL,
                         FOUNDRY_TYPE_FORGE_USER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_forge_issue_init (FoundryForgeIssue *self)
{
}

/**
 * foundry_forge_issue_dup_id:
 * @self: a [class@Foundry.ForgeIssue]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_issue_dup_id (FoundryForgeIssue *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_ISSUE (self), NULL);

  if (FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_id)
    return FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_id (self);

  return NULL;
}

/**
 * foundry_forge_issue_dup_description:
 * @self: a [class@Foundry.ForgeIssue]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_issue_dup_description (FoundryForgeIssue *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_ISSUE (self), NULL);

  if (FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_description)
    return FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_description (self);

  return NULL;
}

/**
 * foundry_forge_issue_dup_online_url:
 * @self: a [class@Foundry.ForgeIssue]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_issue_dup_online_url (FoundryForgeIssue *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_ISSUE (self), NULL);

  if (FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_online_url)
    return FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_online_url (self);

  return NULL;
}

/**
 * foundry_forge_issue_dup_title:
 * @self: a [class@Foundry.ForgeIssue]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_issue_dup_title (FoundryForgeIssue *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_ISSUE (self), NULL);

  if (FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_title)
    return FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_title (self);

  return NULL;
}

/**
 * foundry_forge_issue_dup_state:
 * @self: a [class@Foundry.ForgeIssue]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_issue_dup_state (FoundryForgeIssue *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_ISSUE (self), NULL);

  if (FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_state)
    return FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_state (self);

  return NULL;
}

/**
 * foundry_forge_issue_dup_created_at:
 * @self: a [class@Foundry.ForgeIssue]
 *
 * Returns: (transfer full) (nullable): a #GDateTime
 *
 * Since: 1.1
 */
GDateTime *
foundry_forge_issue_dup_created_at (FoundryForgeIssue *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_ISSUE (self), NULL);

  if (FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_created_at)
    return FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_created_at (self);

  return NULL;
}

/**
 * foundry_forge_issue_dup_author:
 * @self: a [class@Foundry.ForgeIssue]
 *
 * Gets a copy of the user who authored the issue.
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.ForgeUser], or %NULL
 *
 * Since: 1.1
 */
FoundryForgeUser *
foundry_forge_issue_dup_author (FoundryForgeIssue *self)
{
  g_return_val_if_fail (FOUNDRY_IS_FORGE_ISSUE (self), NULL);

  if (FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_author)
    return FOUNDRY_FORGE_ISSUE_GET_CLASS (self)->dup_author (self);

  return NULL;
}
