/* foundry-forge-query.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-forge-query.h"

typedef struct
{
  char *state;
  char *keywords_scope;
  char *keywords;
} FoundryForgeQueryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (FoundryForgeQuery, foundry_forge_query, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_STATE,
  PROP_KEYWORDS_SCOPE,
  PROP_KEYWORDS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_forge_query_finalize (GObject *object)
{
  FoundryForgeQuery *self = (FoundryForgeQuery *)object;
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);

  g_clear_pointer (&priv->state, g_free);
  g_clear_pointer (&priv->keywords_scope, g_free);
  g_clear_pointer (&priv->keywords, g_free);

  G_OBJECT_CLASS (foundry_forge_query_parent_class)->finalize (object);
}

static void
foundry_forge_query_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryForgeQuery *self = FOUNDRY_FORGE_QUERY (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_take_string (value, foundry_forge_query_dup_state (self));
      break;

    case PROP_KEYWORDS_SCOPE:
      g_value_take_string (value, foundry_forge_query_dup_keywords_scope (self));
      break;

    case PROP_KEYWORDS:
      g_value_take_string (value, foundry_forge_query_dup_keywords (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_query_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundryForgeQuery *self = FOUNDRY_FORGE_QUERY (object);

  switch (prop_id)
    {
    case PROP_STATE:
      foundry_forge_query_set_state (self, g_value_get_string (value));
      break;

    case PROP_KEYWORDS_SCOPE:
      foundry_forge_query_set_keywords_scope (self, g_value_get_string (value));
      break;

    case PROP_KEYWORDS:
      foundry_forge_query_set_keywords (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_forge_query_class_init (FoundryForgeQueryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_forge_query_finalize;
  object_class->get_property = foundry_forge_query_get_property;
  object_class->set_property = foundry_forge_query_set_property;

  properties[PROP_STATE] =
    g_param_spec_string ("state", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_KEYWORDS_SCOPE] =
    g_param_spec_string ("keywords-scope", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_KEYWORDS] =
    g_param_spec_string ("keywords", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_forge_query_init (FoundryForgeQuery *self)
{
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);

  priv->state = g_strdup ("open");
  priv->keywords_scope = NULL;
  priv->keywords = NULL;
}

/**
 * foundry_forge_query_new:
 *
 * An empty forge query that does not have specifics provided to any
 * known subsystem filterer.
 *
 * Returns: (transfer full) (not nullable):
 */
FoundryForgeQuery *
foundry_forge_query_new (void)
{
  return g_object_new (FOUNDRY_TYPE_FORGE_QUERY, NULL);
}

/**
 * foundry_forge_query_dup_state:
 * @self: a [class@Foundry.ForgeQuery]
 *
 * Gets the states for the query.
 *
 * Multiple states are supported by separating with a comma.
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_forge_query_dup_state (FoundryForgeQuery *self)
{
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_FORGE_QUERY (self), NULL);

  return g_strdup (priv->state);
}

/**
 * foundry_forge_query_set_state:
 * @self: a [class@Foundry.ForgeQuery]
 *
 * Sets the allowed states for the query.
 *
 * You may specify multiple states with a comma.
 *
 * Since: 1.1
 */
void
foundry_forge_query_set_state (FoundryForgeQuery *self,
                               const char        *state)
{
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_FORGE_QUERY (self));

  if (g_set_str (&priv->state, state))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
}

/**
 * foundry_forge_query_contains_state:
 * @self: a [class@Foundry.ForgeQuery]
 *
 * Helper to check [property@Foundry.ForgeQuery:state] if it contains
 * @state while handling "," separators.
 *
 * Returns: %TRUE if @state was found otherwise %FALSE
 *
 * Since: 1.1
 */
gboolean
foundry_forge_query_contains_state (FoundryForgeQuery *self,
                                    const char        *state)
{
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);
  const char *iter;
  gsize len;

  g_return_val_if_fail (FOUNDRY_IS_FORGE_QUERY (self), FALSE);
  g_return_val_if_fail (state != NULL, FALSE);

  len = strlen (state);
  iter = priv->state;

  while (iter != NULL && iter[0] != 0)
    {
      if ((iter = strstr (iter, state)))
        {
          if (iter[len] == 0 || iter[len] == ',')
            return TRUE;

          iter += len;
        }
    }

  return FALSE;
}

/**
 * foundry_forge_query_dup_keywords_scope:
 * @self: a [class@Foundry.ForgeQuery]
 *
 * Gets the keywords scope for the query.
 *
 * Multiple scopes are supported by separating with a comma.
 *
 * Returns: (transfer full) (nullable): the keywords scope string
 *
 * Since: 1.1
 */
char *
foundry_forge_query_dup_keywords_scope (FoundryForgeQuery *self)
{
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_FORGE_QUERY (self), NULL);

  return g_strdup (priv->keywords_scope);
}

/**
 * foundry_forge_query_set_keywords_scope:
 * @self: a [class@Foundry.ForgeQuery]
 * @keywords_scope: (nullable): the keywords scope to set
 *
 * Sets the keywords scope for the query.
 *
 * You may specify multiple scopes with a comma.
 *
 * Since: 1.1
 */
void
foundry_forge_query_set_keywords_scope (FoundryForgeQuery *self,
                                         const char        *keywords_scope)
{
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_FORGE_QUERY (self));

  if (g_set_str (&priv->keywords_scope, keywords_scope))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEYWORDS_SCOPE]);
}

/**
 * foundry_forge_query_contains_keywords_scope:
 * @self: a [class@Foundry.ForgeQuery]
 * @keywords_scope: the keywords scope to check for
 *
 * Helper to check [property@Foundry.ForgeQuery:keywords-scope] if it contains
 * @keywords_scope while handling "," separators.
 *
 * Returns: %TRUE if @keywords_scope was found otherwise %FALSE
 *
 * Since: 1.1
 */
gboolean
foundry_forge_query_contains_keywords_scope (FoundryForgeQuery *self,
                                              const char        *keywords_scope)
{
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);
  const char *iter;
  gsize len;

  g_return_val_if_fail (FOUNDRY_IS_FORGE_QUERY (self), FALSE);
  g_return_val_if_fail (keywords_scope != NULL, FALSE);

  if (priv->keywords_scope == NULL)
    return FALSE;

  len = strlen (keywords_scope);
  iter = priv->keywords_scope;

  while (iter != NULL && iter[0] != 0)
    {
      if ((iter = strstr (iter, keywords_scope)))
        {
          if (iter[len] == 0 || iter[len] == ',')
            return TRUE;

          iter += len;
        }
    }

  return FALSE;
}

/**
 * foundry_forge_query_dup_keywords:
 * @self: a [class@Foundry.ForgeQuery]
 *
 * Gets the keywords for the query.
 *
 * Returns: (transfer full) (nullable): the keywords string
 *
 * Since: 1.1
 */
char *
foundry_forge_query_dup_keywords (FoundryForgeQuery *self)
{
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_FORGE_QUERY (self), NULL);

  return g_strdup (priv->keywords);
}

/**
 * foundry_forge_query_set_keywords:
 * @self: a [class@Foundry.ForgeQuery]
 * @keywords: (nullable): the keywords to set
 *
 * Sets the keywords for the query.
 *
 * Since: 1.1
 */
void
foundry_forge_query_set_keywords (FoundryForgeQuery *self,
                                  const char        *keywords)
{
  FoundryForgeQueryPrivate *priv = foundry_forge_query_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_FORGE_QUERY (self));

  if (g_set_str (&priv->keywords, keywords))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEYWORDS]);
}
