/* foundry-acp-terminal.c
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

#include "foundry-acp-enums.h"
#include "foundry-acp-terminal-private.h"
#include "foundry-acp-terminal.h"

struct _FoundryAcpTerminal
{
  GObject parent_instance;
  char *id;
  char *turn_id;
  char *command;
  char **argv;
  char *cwd;
  char *latest_output;
  GString *scrollback;
  char *exit_signal;
  gint64 created_at;
  gint64 started_at;
  gint64 exited_at;
  gssize output_byte_limit;
  FoundryAcpTerminalState state;
  int exit_status;
  guint truncated : 1;
  guint has_exit_status : 1;
};

G_DEFINE_FINAL_TYPE (FoundryAcpTerminal, foundry_acp_terminal, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  PROP_TURN_ID,
  PROP_COMMAND,
  PROP_ARGV,
  PROP_CWD,
  PROP_STATE,
  PROP_CREATED_AT,
  PROP_STARTED_AT,
  PROP_EXITED_AT,
  PROP_OUTPUT_BYTE_LIMIT,
  PROP_TRUNCATED,
  PROP_LATEST_OUTPUT,
  PROP_SCROLLBACK,
  PROP_HAS_EXIT_STATUS,
  PROP_EXIT_STATUS,
  PROP_EXIT_SIGNAL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
clear_string (GString *string)
{
  g_string_free (string, TRUE);
}

static void
foundry_acp_terminal_finalize (GObject *object)
{
  FoundryAcpTerminal *self = (FoundryAcpTerminal *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->turn_id, g_free);
  g_clear_pointer (&self->command, g_free);
  g_clear_pointer (&self->argv, g_strfreev);
  g_clear_pointer (&self->cwd, g_free);
  g_clear_pointer (&self->latest_output, g_free);
  g_clear_pointer (&self->scrollback, clear_string);
  g_clear_pointer (&self->exit_signal, g_free);

  G_OBJECT_CLASS (foundry_acp_terminal_parent_class)->finalize (object);
}

static void
foundry_acp_terminal_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryAcpTerminal *self = FOUNDRY_ACP_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_TURN_ID:
      g_value_set_string (value, self->turn_id);
      break;

    case PROP_COMMAND:
      g_value_set_string (value, self->command);
      break;

    case PROP_ARGV:
      g_value_set_boxed (value, self->argv);
      break;

    case PROP_CWD:
      g_value_set_string (value, self->cwd);
      break;

    case PROP_STATE:
      g_value_set_enum (value, self->state);
      break;

    case PROP_CREATED_AT:
      g_value_set_int64 (value, self->created_at);
      break;

    case PROP_STARTED_AT:
      g_value_set_int64 (value, self->started_at);
      break;

    case PROP_EXITED_AT:
      g_value_set_int64 (value, self->exited_at);
      break;

    case PROP_OUTPUT_BYTE_LIMIT:
      g_value_set_int64 (value, self->output_byte_limit);
      break;

    case PROP_TRUNCATED:
      g_value_set_boolean (value, self->truncated);
      break;

    case PROP_LATEST_OUTPUT:
      g_value_set_string (value, self->latest_output);
      break;

    case PROP_SCROLLBACK:
      g_value_set_string (value, self->scrollback != NULL ? self->scrollback->str : NULL);
      break;

    case PROP_HAS_EXIT_STATUS:
      g_value_set_boolean (value, self->has_exit_status);
      break;

    case PROP_EXIT_STATUS:
      g_value_set_int (value, self->exit_status);
      break;

    case PROP_EXIT_SIGNAL:
      g_value_set_string (value, self->exit_signal);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_acp_terminal_class_init (FoundryAcpTerminalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_terminal_finalize;
  object_class->get_property = foundry_acp_terminal_get_property;

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TURN_ID] =
    g_param_spec_string ("turn-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_COMMAND] =
    g_param_spec_string ("command", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_ARGV] =
    g_param_spec_boxed ("argv", NULL, NULL, G_TYPE_STRV,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_CWD] =
    g_param_spec_string ("cwd", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_STATE] =
    g_param_spec_enum ("state", NULL, NULL,
                       FOUNDRY_TYPE_ACP_TERMINAL_STATE,
                       FOUNDRY_ACP_TERMINAL_RUNNING,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_CREATED_AT] =
    g_param_spec_int64 ("created-at", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_STARTED_AT] =
    g_param_spec_int64 ("started-at", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_EXITED_AT] =
    g_param_spec_int64 ("exited-at", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_OUTPUT_BYTE_LIMIT] =
    g_param_spec_int64 ("output-byte-limit", NULL, NULL,
                        -1, G_MAXINT64, -1,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TRUNCATED] =
    g_param_spec_boolean ("truncated", NULL, NULL, FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_LATEST_OUTPUT] =
    g_param_spec_string ("latest-output", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SCROLLBACK] =
    g_param_spec_string ("scrollback", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_HAS_EXIT_STATUS] =
    g_param_spec_boolean ("has-exit-status", NULL, NULL, FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_EXIT_STATUS] =
    g_param_spec_int ("exit-status", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_EXIT_SIGNAL] =
    g_param_spec_string ("exit-signal", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_acp_terminal_init (FoundryAcpTerminal *self)
{
  self->created_at = g_get_real_time ();
  self->output_byte_limit = -1;
  self->scrollback = g_string_new (NULL);
  self->state = FOUNDRY_ACP_TERMINAL_RUNNING;
}

/**
 * foundry_acp_terminal_new:
 * @id: the protocol terminal identifier
 *
 * Creates a terminal handle for ACP terminal APIs.
 *
 * Returns: (transfer full): a [class@Foundry.AcpTerminal]
 *
 * Since: 1.2
 */
FoundryAcpTerminal *
foundry_acp_terminal_new (const char *id)
{
  FoundryAcpTerminal *self;

  g_return_val_if_fail (id != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_TERMINAL, NULL);
  self->id = g_strdup (id);

  return self;
}

/**
 * foundry_acp_terminal_dup_id:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: (transfer full): the protocol terminal identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_terminal_dup_id (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), NULL);

  return g_strdup (self->id);
}

/**
 * foundry_acp_terminal_dup_turn_id:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: (transfer full) (nullable): the prompt turn identifier that
 *   created the terminal
 *
 * Since: 1.2
 */
char *
foundry_acp_terminal_dup_turn_id (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), NULL);

  return g_strdup (self->turn_id);
}

/**
 * foundry_acp_terminal_dup_command:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: (transfer full) (nullable): the command used to create the terminal
 *
 * Since: 1.2
 */
char *
foundry_acp_terminal_dup_command (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), NULL);

  return g_strdup (self->command);
}

/**
 * foundry_acp_terminal_dup_argv:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8)
 *   (nullable): the argument vector used to create the terminal
 *
 * Since: 1.2
 */
char **
foundry_acp_terminal_dup_argv (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), NULL);

  return g_strdupv (self->argv);
}

/**
 * foundry_acp_terminal_dup_cwd:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: (transfer full) (nullable): the working directory for the terminal
 *
 * Since: 1.2
 */
char *
foundry_acp_terminal_dup_cwd (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), NULL);

  return g_strdup (self->cwd);
}

/**
 * foundry_acp_terminal_get_state:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: the terminal state
 *
 * Since: 1.2
 */
FoundryAcpTerminalState
foundry_acp_terminal_get_state (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), FOUNDRY_ACP_TERMINAL_FAILED);

  return self->state;
}

/**
 * foundry_acp_terminal_get_created_at:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: wall-clock creation time in microseconds since the Unix epoch
 *
 * Since: 1.2
 */
gint64
foundry_acp_terminal_get_created_at (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), 0);

  return self->created_at;
}

/**
 * foundry_acp_terminal_get_started_at:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: wall-clock subprocess start time in microseconds since the Unix
 *   epoch, or zero if unknown
 *
 * Since: 1.2
 */
gint64
foundry_acp_terminal_get_started_at (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), 0);

  return self->started_at;
}

/**
 * foundry_acp_terminal_get_exited_at:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: wall-clock subprocess exit time in microseconds since the Unix
 *   epoch, or zero if the terminal has not exited
 *
 * Since: 1.2
 */
gint64
foundry_acp_terminal_get_exited_at (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), 0);

  return self->exited_at;
}

/**
 * foundry_acp_terminal_get_output_byte_limit:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: the configured output byte limit, or -1 for the default
 *
 * Since: 1.2
 */
gssize
foundry_acp_terminal_get_output_byte_limit (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), -1);

  return self->output_byte_limit;
}

/**
 * foundry_acp_terminal_get_truncated:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: %TRUE if output has been truncated
 *
 * Since: 1.2
 */
gboolean
foundry_acp_terminal_get_truncated (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), FALSE);

  return self->truncated;
}

/**
 * foundry_acp_terminal_dup_latest_output:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: (transfer full) (nullable): the most recent terminal output text
 *
 * Since: 1.2
 */
char *
foundry_acp_terminal_dup_latest_output (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), NULL);

  return g_strdup (self->latest_output);
}

/**
 * foundry_acp_terminal_dup_scrollback:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: (transfer full): accumulated terminal output
 *
 * Since: 1.2
 */
char *
foundry_acp_terminal_dup_scrollback (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), NULL);

  return g_strdup (self->scrollback != NULL ? self->scrollback->str : "");
}

/**
 * foundry_acp_terminal_has_exit_status:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: %TRUE if an exit status is available
 *
 * Since: 1.2
 */
gboolean
foundry_acp_terminal_has_exit_status (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), FALSE);

  return self->has_exit_status;
}

/**
 * foundry_acp_terminal_get_exit_status:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: the exit status, or zero if none has been reported
 *
 * Since: 1.2
 */
int
foundry_acp_terminal_get_exit_status (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), 0);

  return self->exit_status;
}

/**
 * foundry_acp_terminal_dup_exit_signal:
 * @self: a [class@Foundry.AcpTerminal]
 *
 * Returns: (transfer full) (nullable): the exit signal name, if any
 *
 * Since: 1.2
 */
char *
foundry_acp_terminal_dup_exit_signal (FoundryAcpTerminal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_TERMINAL (self), NULL);

  return g_strdup (self->exit_signal);
}

void
_foundry_acp_terminal_set_turn_id (FoundryAcpTerminal *self,
                                   const char         *turn_id)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  if (g_set_str (&self->turn_id, turn_id))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TURN_ID]);
}

void
_foundry_acp_terminal_set_command (FoundryAcpTerminal *self,
                                   const char         *command)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  if (g_set_str (&self->command, command))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND]);
}

void
_foundry_acp_terminal_set_argv (FoundryAcpTerminal *self,
                                const char * const *argv)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  g_strfreev (self->argv);
  self->argv = g_strdupv ((char **)argv);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ARGV]);
}

void
_foundry_acp_terminal_set_cwd (FoundryAcpTerminal *self,
                               const char         *cwd)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  if (g_set_str (&self->cwd, cwd))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CWD]);
}

void
_foundry_acp_terminal_set_state (FoundryAcpTerminal      *self,
                                 FoundryAcpTerminalState  state)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  if (self->state != state)
    {
      self->state = state;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
    }
}

void
_foundry_acp_terminal_set_started_at (FoundryAcpTerminal *self,
                                      gint64              started_at)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  if (self->started_at != started_at)
    {
      self->started_at = started_at;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STARTED_AT]);
    }
}

void
_foundry_acp_terminal_set_exited_at (FoundryAcpTerminal *self,
                                     gint64              exited_at)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  if (self->exited_at != exited_at)
    {
      self->exited_at = exited_at;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXITED_AT]);
    }
}

void
_foundry_acp_terminal_set_output_byte_limit (FoundryAcpTerminal *self,
                                             gssize              output_byte_limit)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  if (self->output_byte_limit != output_byte_limit)
    {
      self->output_byte_limit = output_byte_limit;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OUTPUT_BYTE_LIMIT]);
    }
}

void
_foundry_acp_terminal_set_truncated (FoundryAcpTerminal *self,
                                     gboolean            truncated)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  truncated = !!truncated;

  if (self->truncated != truncated)
    {
      self->truncated = truncated;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRUNCATED]);
    }
}

void
_foundry_acp_terminal_apply_output (FoundryAcpTerminal       *self,
                                    FoundryAcpTerminalOutput *output)
{
  g_autofree char *exit_signal = NULL;
  g_autofree char *text = NULL;
  gboolean has_exit_status;
  int exit_status;

  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));
  g_return_if_fail (output != NULL);

  text = foundry_acp_terminal_output_dup_text (output);
  has_exit_status = foundry_acp_terminal_output_has_exit_status (output);
  exit_status = foundry_acp_terminal_output_get_exit_code (output);
  exit_signal = foundry_acp_terminal_output_dup_signal (output);

  _foundry_acp_terminal_set_truncated (self,
                                       foundry_acp_terminal_output_get_truncated (output));

  if (g_set_str (&self->latest_output, text))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LATEST_OUTPUT]);

  if (text != NULL && text[0] != 0)
    {
      g_string_append (self->scrollback, text);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCROLLBACK]);
    }

  if (has_exit_status != self->has_exit_status)
    {
      self->has_exit_status = has_exit_status;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_EXIT_STATUS]);
    }

  if (self->exit_status != exit_status)
    {
      self->exit_status = exit_status;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXIT_STATUS]);
    }

  if (g_set_str (&self->exit_signal, exit_signal))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXIT_SIGNAL]);

  if (has_exit_status)
    {
      _foundry_acp_terminal_set_exited_at (self, g_get_real_time ());
      _foundry_acp_terminal_set_state (self, FOUNDRY_ACP_TERMINAL_EXITED);
    }
}

void
_foundry_acp_terminal_clear_exit_status (FoundryAcpTerminal *self)
{
  g_return_if_fail (FOUNDRY_IS_ACP_TERMINAL (self));

  if (self->has_exit_status)
    {
      self->has_exit_status = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_EXIT_STATUS]);
    }

  if (self->exit_status != 0)
    {
      self->exit_status = 0;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXIT_STATUS]);
    }

  if (g_set_str (&self->exit_signal, NULL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXIT_SIGNAL]);
}
