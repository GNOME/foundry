/* foundry-acp-terminal-output.c
 *
 * Copyright 2026 Christian Hergert
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-acp-terminal-output-private.h"
#include "foundry-json-node.h"

struct _FoundryAcpTerminalOutput
{
  GObject parent_instance;

  char *text;
  char *signal_name;
  int exit_code;
  guint truncated : 1;
  guint has_exit_status : 1;
};

G_DEFINE_FINAL_TYPE (FoundryAcpTerminalOutput, foundry_acp_terminal_output, G_TYPE_OBJECT)

static void
foundry_acp_terminal_output_finalize (GObject *object)
{
  FoundryAcpTerminalOutput *self = (FoundryAcpTerminalOutput *)object;

  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->signal_name, g_free);

  G_OBJECT_CLASS (foundry_acp_terminal_output_parent_class)->finalize (object);
}

static void
foundry_acp_terminal_output_class_init (FoundryAcpTerminalOutputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_terminal_output_finalize;
}

static void
foundry_acp_terminal_output_init (FoundryAcpTerminalOutput *self)
{
}

/**
 * foundry_acp_terminal_output_new:
 * @text: terminal output text
 * @truncated: if the output was truncated
 * @has_exit_status: if @exit_code or @signal_name is meaningful
 * @exit_code: process exit code
 * @signal_name: (nullable): process signal name
 *
 * Creates a terminal output snapshot.
 *
 * Returns: (transfer full): a [class@Foundry.AcpTerminalOutput]
 *
 * Since: 1.2
 */
FoundryAcpTerminalOutput *
foundry_acp_terminal_output_new (const char *text,
                                 gboolean    truncated,
                                 gboolean    has_exit_status,
                                 int         exit_code,
                                 const char *signal_name)
{
  FoundryAcpTerminalOutput *self;

  g_return_val_if_fail (text != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_TERMINAL_OUTPUT, NULL);
  self->text = g_strdup (text);
  self->signal_name = g_strdup (signal_name);
  self->exit_code = exit_code;
  self->truncated = !!truncated;
  self->has_exit_status = !!has_exit_status;

  return self;
}

/**
 * foundry_acp_terminal_output_dup_text:
 * @self: a [class@Foundry.AcpTerminalOutput]
 *
 * Returns: (transfer full): the terminal output
 *
 * Since: 1.2
 */
char *
foundry_acp_terminal_output_dup_text (FoundryAcpTerminalOutput *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL_OUTPUT (self), NULL);

  return g_strdup (self->text);
}

/**
 * foundry_acp_terminal_output_get_truncated:
 * @self: a [class@Foundry.AcpTerminalOutput]
 *
 * Returns: %TRUE if output was truncated
 *
 * Since: 1.2
 */
gboolean
foundry_acp_terminal_output_get_truncated (FoundryAcpTerminalOutput *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL_OUTPUT (self), FALSE);

  return self->truncated;
}

/**
 * foundry_acp_terminal_output_has_exit_status:
 * @self: a [class@Foundry.AcpTerminalOutput]
 *
 * Returns: %TRUE if an exit status is available
 *
 * Since: 1.2
 */
gboolean
foundry_acp_terminal_output_has_exit_status (FoundryAcpTerminalOutput *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL_OUTPUT (self), FALSE);

  return self->has_exit_status;
}

/**
 * foundry_acp_terminal_output_get_exit_code:
 * @self: a [class@Foundry.AcpTerminalOutput]
 *
 * Returns: the exit code
 *
 * Since: 1.2
 */
int
foundry_acp_terminal_output_get_exit_code (FoundryAcpTerminalOutput *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL_OUTPUT (self), 0);

  return self->exit_code;
}

/**
 * foundry_acp_terminal_output_dup_signal:
 * @self: a [class@Foundry.AcpTerminalOutput]
 *
 * Returns: (transfer full) (nullable): the signal name
 *
 * Since: 1.2
 */
char *
foundry_acp_terminal_output_dup_signal (FoundryAcpTerminalOutput *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL_OUTPUT (self), NULL);

  return g_strdup (self->signal_name);
}

JsonNode *
_foundry_acp_terminal_output_to_json (FoundryAcpTerminalOutput *self)
{
  g_autoptr(JsonNode) exit_status = NULL;

  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL_OUTPUT (self), NULL);

  if (self->has_exit_status)
    {
      if (self->signal_name != NULL)
        exit_status = FOUNDRY_JSON_OBJECT_NEW ("signal",
                                               FOUNDRY_JSON_NODE_PUT_STRING (self->signal_name));
      else
        exit_status = FOUNDRY_JSON_OBJECT_NEW ("exitCode",
                                               FOUNDRY_JSON_NODE_PUT_INT (self->exit_code));
    }

  return FOUNDRY_JSON_OBJECT_NEW ("output", FOUNDRY_JSON_NODE_PUT_STRING (self->text),
                                  "truncated", FOUNDRY_JSON_NODE_PUT_BOOLEAN (self->truncated),
                                  "exitStatus", FOUNDRY_JSON_NODE_PUT_NODE (exit_status));
}
