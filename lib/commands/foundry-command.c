/* foundry-command.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "foundry-command-provider.h"
#include "foundry-command-private.h"

typedef struct
{
  GWeakRef provider_wr;
  char *id;
  char *name;
} FoundryCommandPrivate;

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  N_PROPS
};

G_DEFINE_TYPE (FoundryCommand, foundry_command, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_command_finalize (GObject *object)
{
  FoundryCommand *self = (FoundryCommand *)object;
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_weak_ref_clear (&priv->provider_wr);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (foundry_command_parent_class)->finalize (object);
}

static void
foundry_command_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  FoundryCommand *self = FOUNDRY_COMMAND (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, foundry_command_dup_id (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, foundry_command_dup_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_command_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  FoundryCommand *self = FOUNDRY_COMMAND (object);

  switch (prop_id)
    {
    case PROP_ID:
      foundry_command_set_id (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      foundry_command_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_command_class_init (FoundryCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_command_finalize;
  object_class->get_property = foundry_command_get_property;
  object_class->set_property = foundry_command_set_property;

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_command_init (FoundryCommand *self)
{
}

char *
foundry_command_dup_id (FoundryCommand *self)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), NULL);

  return g_strdup (priv->id);
}

void
foundry_command_set_id (FoundryCommand *self,
                        const char     *id)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_COMMAND (self));

  if (g_set_str (&priv->id, id))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
}

char *
foundry_command_dup_name (FoundryCommand *self)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), NULL);

  return g_strdup (priv->name);
}

void
foundry_command_set_name (FoundryCommand *self,
                          const char     *name)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_COMMAND (self));

  if (g_set_str (&priv->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

/**
 * foundry_command_can_default:
 * @self: a [class@Foundry.Command]
 * @priority: (out) (nullable): a location for the priority, or %NULL
 *
 * Checks to see if @self is suitable to be run as the default command when
 * running a project. The priority indicates if it should take priority over
 * other commands which can be default. The highest priority value wins.
 *
 * Returns: %TRUE if @self can be the default command
 */
gboolean
foundry_command_can_default (FoundryCommand *self,
                             guint          *priority)
{
  guint local;

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), FALSE);

  if (priority == NULL)
    priority = &local;

  if (FOUNDRY_COMMAND_GET_CLASS (self)->can_default)
    return FOUNDRY_COMMAND_GET_CLASS (self)->can_default (self, priority);

  return FALSE;
}

FoundryCommandProvider *
_foundry_command_dup_provider (FoundryCommand *self)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), NULL);

  return g_weak_ref_get (&priv->provider_wr);
}

void
_foundry_command_set_provider (FoundryCommand         *self,
                               FoundryCommandProvider *provider)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_COMMAND (self));
  g_return_if_fail (!provider || FOUNDRY_IS_COMMAND_PROVIDER (provider));

  g_weak_ref_set (&priv->provider_wr, provider);
}
