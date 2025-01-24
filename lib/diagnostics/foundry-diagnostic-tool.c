/* foundry-diagnostic-tool.c
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

#include "foundry-command.h"
#include "foundry-diagnostic-tool.h"

typedef struct
{
  FoundryCommand *command;
} FoundryDiagnosticToolPrivate;

enum {
  PROP_0,
  PROP_COMMAND,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (FoundryDiagnosticTool, foundry_diagnostic_tool, FOUNDRY_TYPE_DIAGNOSTIC_PROVIDER)

static GParamSpec *properties[N_PROPS];

static void
foundry_diagnostic_tool_finalize (GObject *object)
{
  FoundryDiagnosticTool *self = (FoundryDiagnosticTool *)object;
  FoundryDiagnosticToolPrivate *priv = foundry_diagnostic_tool_get_instance_private (self);

  g_clear_object (&priv->command);

  G_OBJECT_CLASS (foundry_diagnostic_tool_parent_class)->finalize (object);
}

static void
foundry_diagnostic_tool_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  FoundryDiagnosticTool *self = FOUNDRY_DIAGNOSTIC_TOOL (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_take_object (value, foundry_diagnostic_tool_dup_command (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_diagnostic_tool_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  FoundryDiagnosticTool *self = FOUNDRY_DIAGNOSTIC_TOOL (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      foundry_diagnostic_tool_set_command (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_diagnostic_tool_class_init (FoundryDiagnosticToolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_diagnostic_tool_finalize;
  object_class->get_property = foundry_diagnostic_tool_get_property;
  object_class->set_property = foundry_diagnostic_tool_set_property;

  properties[PROP_COMMAND] =
    g_param_spec_object ("command", NULL, NULL,
                         FOUNDRY_TYPE_COMMAND,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_diagnostic_tool_init (FoundryDiagnosticTool *self)
{
}

/**
 * foundry_diagnostic_tool_dup_command:
 * @self: a #FoundryDiagnosticTool
 *
 * Returns: (transfer full) (nullable): a #FoundryCommand or %NULL
 */
FoundryCommand *
foundry_diagnostic_tool_dup_command (FoundryDiagnosticTool *self)
{
  FoundryDiagnosticToolPrivate *priv = foundry_diagnostic_tool_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DIAGNOSTIC_TOOL (self), NULL);

  return priv->command ? g_object_ref (priv->command) : NULL;
}

void
foundry_diagnostic_tool_set_command (FoundryDiagnosticTool *self,
                                     FoundryCommand        *command)
{
  FoundryDiagnosticToolPrivate *priv = foundry_diagnostic_tool_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_DIAGNOSTIC_TOOL (self));
  g_return_if_fail (!command || FOUNDRY_IS_COMMAND (command));

  if (g_set_object (&priv->command, command))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND]);
}
