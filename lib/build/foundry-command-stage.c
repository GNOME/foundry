/* foundry-command-stage.c
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

#include "foundry-command-stage.h"

struct _FoundryCommandStage
{
  FoundryBuildStage  parent_instance;
  FoundryCommand    *build_command;
  FoundryCommand    *clean_command;
};

enum {
  PROP_0,
  PROP_BUILD_COMMAND,
  PROP_CLEAN_COMMAND,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryCommandStage, foundry_command_stage, FOUNDRY_TYPE_BUILD_STAGE)

static GParamSpec *properties[N_PROPS];

static void
foundry_command_stage_dispose (GObject *object)
{
  FoundryCommandStage *self = (FoundryCommandStage *)object;

  g_clear_object (&self->build_command);
  g_clear_object (&self->clean_command);

  G_OBJECT_CLASS (foundry_command_stage_parent_class)->dispose (object);
}

static void
foundry_command_stage_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryCommandStage *self = FOUNDRY_COMMAND_STAGE (object);

  switch (prop_id)
    {
    case PROP_BUILD_COMMAND:
      g_value_take_object (value, foundry_command_stage_dup_build_command (self));
      break;

    case PROP_CLEAN_COMMAND:
      g_value_take_object (value, foundry_command_stage_dup_clean_command (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_command_stage_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  FoundryCommandStage *self = FOUNDRY_COMMAND_STAGE (object);

  switch (prop_id)
    {
    case PROP_BUILD_COMMAND:
      self->build_command = g_value_dup_object (value);
      break;

    case PROP_CLEAN_COMMAND:
      self->clean_command = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_command_stage_class_init (FoundryCommandStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_command_stage_dispose;
  object_class->get_property = foundry_command_stage_get_property;
  object_class->set_property = foundry_command_stage_set_property;

  properties[PROP_BUILD_COMMAND] =
    g_param_spec_object ("build-command", NULL, NULL,
                         FOUNDRY_TYPE_COMMAND,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_CLEAN_COMMAND] =
    g_param_spec_object ("clean-command", NULL, NULL,
                         FOUNDRY_TYPE_COMMAND,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_command_stage_init (FoundryCommandStage *self)
{
}

/**
 * foundry_command_stage_dup_build_command:
 * @self: a [class@Foundry.CommandStage]
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.Command]
 */
FoundryCommand *
foundry_command_stage_dup_build_command (FoundryCommandStage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_STAGE (self), NULL);

  return self->build_command ? g_object_ref (self->build_command) : NULL;
}

/**
 * foundry_command_stage_dup_clean_command:
 * @self: a [class@Foundry.CommandStage]
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.Command]
 */
FoundryCommand *
foundry_command_stage_dup_clean_command (FoundryCommandStage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_STAGE (self), NULL);

  return self->clean_command ? g_object_ref (self->clean_command) : NULL;
}

FoundryBuildStage *
foundry_command_stage_new (FoundryContext *context,
                           FoundryCommand *build_command,
                           FoundryCommand *clean_command)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (!build_command || FOUNDRY_IS_COMMAND (build_command), NULL);
  g_return_val_if_fail (!clean_command || FOUNDRY_IS_COMMAND (clean_command), NULL);

  return g_object_new (FOUNDRY_TYPE_COMMAND_STAGE,
                       "context", context,
                       "build-command", build_command,
                       "clean-command", clean_command,
                       NULL);
}
