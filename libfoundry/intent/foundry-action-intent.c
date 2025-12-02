/* foundry-action-intent.c
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

#include "foundry-action-intent.h"

/**
 * FoundryActionIntent:
 *
 * Represents an intent to perform an action with optional parameters.
 *
 * FoundryActionIntent provides functionality for representing actions
 * that can be performed within the development environment. It stores
 * an action name and optional target parameters as a GVariant.
 *
 * Since: 1.1
 */

struct _FoundryActionIntent
{
  FoundryIntent  parent_instance;
  char          *action_name;
  GVariant      *action_target;
};

enum {
  PROP_0,
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryActionIntent, foundry_action_intent, FOUNDRY_TYPE_INTENT)

static GParamSpec *properties[N_PROPS];

static void
foundry_action_intent_finalize (GObject *object)
{
  FoundryActionIntent *self = FOUNDRY_ACTION_INTENT (object);

  g_clear_pointer (&self->action_name, g_free);
  g_clear_pointer (&self->action_target, g_variant_unref);

  G_OBJECT_CLASS (foundry_action_intent_parent_class)->finalize (object);
}

static void
foundry_action_intent_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryActionIntent *self = FOUNDRY_ACTION_INTENT (object);

  switch (prop_id)
    {
    case PROP_ACTION_NAME:
      g_value_take_string (value, foundry_action_intent_dup_action_name (self));
      break;

    case PROP_ACTION_TARGET:
      g_value_take_variant (value, foundry_action_intent_dup_action_target (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_action_intent_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  FoundryActionIntent *self = FOUNDRY_ACTION_INTENT (object);

  switch (prop_id)
    {
    case PROP_ACTION_NAME:
      self->action_name = g_value_dup_string (value);
      break;

    case PROP_ACTION_TARGET:
      self->action_target = g_value_dup_variant (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_action_intent_class_init (FoundryActionIntentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_action_intent_finalize;
  object_class->get_property = foundry_action_intent_get_property;
  object_class->set_property = foundry_action_intent_set_property;

  properties[PROP_ACTION_NAME] =
    g_param_spec_string ("action-name", NULL, NULL,
                         NULL,
                         (G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ACTION_TARGET] =
    g_param_spec_variant ("action-target", NULL, NULL,
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          (G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_action_intent_init (FoundryActionIntent *self)
{
}

/**
 * foundry_action_intent_new:
 * @action_name: the name of the action to perform
 * @action_target: (nullable): optional parameters for the action as a GVariant
 *
 * Creates a new [class@Foundry.ActionIntent] for performing an action
 * with optional parameters.
 *
 * If @action_target is floating, its floating reference is sunk.
 *
 * Returns: (transfer full): a new [class@Foundry.ActionIntent]
 *
 * Since: 1.1
 */
FoundryIntent *
foundry_action_intent_new (const char *action_name,
                           GVariant   *action_target)
{
  g_autoptr(GVariant) sink = NULL;

  g_return_val_if_fail (action_name != NULL, NULL);

  if (action_target)
    sink = g_variant_ref_sink (action_target);

  return g_object_new (FOUNDRY_TYPE_ACTION_INTENT,
                       "action-name", action_name,
                       "action-target", action_target,
                       NULL);
}

/**
 * foundry_action_intent_dup_action_name:
 * @self: a [class@Foundry.ActionIntent]
 *
 * Gets a copy of the action name.
 *
 * Returns: (transfer full) (not nullable): a newly allocated copy of the action name
 *
 * Since: 1.1
 */
char *
foundry_action_intent_dup_action_name (FoundryActionIntent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACTION_INTENT (self), NULL);

  return g_strdup (self->action_name);
}

/**
 * foundry_action_intent_dup_action_target:
 * @self: a [class@Foundry.ActionIntent]
 *
 * Gets a copy of the action target parameters.
 *
 * Returns: (transfer full) (nullable): a [type@GVariant] with the action target,
 *   or %NULL if no target is set
 *
 * Since: 1.1
 */
GVariant *
foundry_action_intent_dup_action_target (FoundryActionIntent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACTION_INTENT (self), NULL);

  if (self->action_target != NULL)
    return g_variant_ref (self->action_target);

  return NULL;
}
