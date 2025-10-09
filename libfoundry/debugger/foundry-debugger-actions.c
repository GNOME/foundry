/* foundry-debugger-actions.c
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

#include "foundry-debugger.h"
#include "foundry-debugger-actions.h"
#include "foundry-debugger-thread.h"
#include "foundry-util.h"

#include "eggactiongroup.h"

struct _FoundryDebuggerActions
{
  GObject                parent_instance;
  FoundryDebugger       *debugger;
  FoundryDebuggerThread *thread;
  gulong                 thread_changed_id;
};

enum {
  PROP_0,
  PROP_DEBUGGER,
  PROP_THREAD,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_debugger_actions_move_action (FoundryDebuggerActions *self,
                                      GVariant               *param)
{
  FoundryDebuggerMovement movement = FOUNDRY_DEBUGGER_MOVEMENT_CONTINUE;
  const gchar *movement_str = NULL;

  g_assert (FOUNDRY_IS_DEBUGGER_ACTIONS (self));

  if (param != NULL)
    movement_str = g_variant_get_string (param, NULL);

  if (g_strcmp0 (movement_str, "step-in") == 0)
    movement = FOUNDRY_DEBUGGER_MOVEMENT_STEP_IN;
  else if (g_strcmp0 (movement_str, "step-out") == 0)
    movement = FOUNDRY_DEBUGGER_MOVEMENT_STEP_OUT;
  else if (g_strcmp0 (movement_str, "step-over") == 0)
    movement = FOUNDRY_DEBUGGER_MOVEMENT_STEP_OVER;
  else if (g_strcmp0 (movement_str, "continue") == 0)
    movement = FOUNDRY_DEBUGGER_MOVEMENT_CONTINUE;

  if (self->thread != NULL)
    dex_future_disown (foundry_debugger_thread_move (self->thread, movement));
}

static void
foundry_debugger_actions_interrupt_action (FoundryDebuggerActions *self,
                                           GVariant               *param)
{
  g_assert (FOUNDRY_IS_DEBUGGER_ACTIONS (self));

  if (self->thread != NULL)
    dex_future_disown (foundry_debugger_thread_interrupt (self->thread));
}

EGG_DEFINE_ACTION_GROUP (FoundryDebuggerActions, foundry_debugger_actions, {
  { "move", foundry_debugger_actions_move_action, "s", NULL },
  { "interrupt", foundry_debugger_actions_interrupt_action, NULL, NULL },
})

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryDebuggerActions, foundry_debugger_actions, G_TYPE_OBJECT,
                                EGG_IMPLEMENT_ACTION_GROUP (foundry_debugger_actions))

static void
foundry_debugger_actions_thread_changed_cb (FoundryDebuggerActions *self,
                                            GParamSpec             *pspec,
                                            FoundryDebuggerThread  *thread)
{
  gboolean is_stopped = TRUE;

  g_assert (!thread || FOUNDRY_IS_DEBUGGER_THREAD (thread));
  g_assert (FOUNDRY_IS_DEBUGGER_ACTIONS (self));

  if (thread != NULL)
    is_stopped = foundry_debugger_thread_is_stopped (thread);

  foundry_debugger_actions_set_action_enabled (self, "move", is_stopped);
  foundry_debugger_actions_set_action_enabled (self, "interrupt", !is_stopped);
}

static void
foundry_debugger_actions_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundryDebuggerActions *self = FOUNDRY_DEBUGGER_ACTIONS (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      g_value_take_object (value, foundry_debugger_actions_dup_debugger (self));
      break;

    case PROP_THREAD:
      g_value_take_object (value, foundry_debugger_actions_dup_thread (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_debugger_actions_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  FoundryDebuggerActions *self = FOUNDRY_DEBUGGER_ACTIONS (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      foundry_debugger_actions_set_debugger (self, g_value_get_object (value));
      break;

    case PROP_THREAD:
      foundry_debugger_actions_set_thread (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_debugger_actions_dispose (GObject *object)
{
  FoundryDebuggerActions *self = FOUNDRY_DEBUGGER_ACTIONS (object);

  g_clear_signal_handler (&self->thread_changed_id, self->thread);

  g_clear_object (&self->thread);
  g_clear_object (&self->debugger);

  G_OBJECT_CLASS (foundry_debugger_actions_parent_class)->dispose (object);
}

static void
foundry_debugger_actions_class_init (FoundryDebuggerActionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_debugger_actions_get_property;
  object_class->set_property = foundry_debugger_actions_set_property;
  object_class->dispose = foundry_debugger_actions_dispose;

  /**
   * FoundryDebuggerActions:debugger:
   *
   * The debugger instance.
   *
   * Since: 1.1
   */
  properties[PROP_DEBUGGER] =
    g_param_spec_object ("debugger", NULL, NULL,
                         FOUNDRY_TYPE_DEBUGGER,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryDebuggerActions:thread:
   *
   * The debugger thread.
   *
   * Since: 1.1
   */
  properties[PROP_THREAD] =
    g_param_spec_object ("thread", NULL, NULL,
                         FOUNDRY_TYPE_DEBUGGER_THREAD,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_debugger_actions_init (FoundryDebuggerActions *self)
{
}

/**
 * foundry_debugger_actions_new:
 * @debugger: (nullable): a [class@Foundry.Debugger]
 * @thread: (nullable): a [class@Foundry.DebuggerThread]
 *
 * Creates a new [class@Foundry.DebuggerActions] instance.
 *
 * Returns: (transfer full): a new [class@Foundry.DebuggerActions]
 *
 * Since: 1.1
 */
FoundryDebuggerActions *
foundry_debugger_actions_new (FoundryDebugger       *debugger,
                              FoundryDebuggerThread *thread)
{
  g_return_val_if_fail (!debugger || FOUNDRY_IS_DEBUGGER (debugger), NULL);
  g_return_val_if_fail (!thread || FOUNDRY_IS_DEBUGGER_THREAD (thread), NULL);

  return g_object_new (FOUNDRY_TYPE_DEBUGGER_ACTIONS,
                       "debugger", debugger,
                       "thread", thread,
                       NULL);
}

/**
 * foundry_debugger_actions_dup_debugger:
 * @self: a [class@Foundry.DebuggerActions]
 *
 * Gets the debugger instance.
 *
 * Returns: (transfer full) (nullable): the debugger instance
 *
 * Since: 1.1
 */
FoundryDebugger *
foundry_debugger_actions_dup_debugger (FoundryDebuggerActions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER_ACTIONS (self), NULL);

  return self->debugger ? g_object_ref (self->debugger) : NULL;
}

/**
 * foundry_debugger_actions_set_debugger:
 * @self: a [class@Foundry.DebuggerActions]
 * @debugger: (nullable): the debugger instance
 *
 * Sets the debugger instance.
 *
 * Since: 1.1
 */
void
foundry_debugger_actions_set_debugger (FoundryDebuggerActions *self,
                                       FoundryDebugger       *debugger)
{
  g_return_if_fail (FOUNDRY_IS_DEBUGGER_ACTIONS (self));
  g_return_if_fail (debugger == NULL || FOUNDRY_IS_DEBUGGER (debugger));

  if (g_set_object (&self->debugger, debugger))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEBUGGER]);
}

/**
 * foundry_debugger_actions_dup_thread:
 * @self: a [class@Foundry.DebuggerActions]
 *
 * Gets the debugger thread.
 *
 * Returns: (transfer full) (nullable): the debugger thread
 *
 * Since: 1.1
 */
FoundryDebuggerThread *
foundry_debugger_actions_dup_thread (FoundryDebuggerActions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER_ACTIONS (self), NULL);

  return self->thread ? g_object_ref (self->thread) : NULL;
}

/**
 * foundry_debugger_actions_set_thread:
 * @self: a [class@Foundry.DebuggerActions]
 * @thread: (nullable): the debugger thread
 *
 * Sets the debugger thread and connects to its "changed" signal
 * to update action states.
 *
 * Since: 1.1
 */
void
foundry_debugger_actions_set_thread (FoundryDebuggerActions *self,
                                     FoundryDebuggerThread  *thread)
{
  g_return_if_fail (FOUNDRY_IS_DEBUGGER_ACTIONS (self));
  g_return_if_fail (thread == NULL || FOUNDRY_IS_DEBUGGER_THREAD (thread));

  if (self->thread == thread)
    return;

  if (thread)
    g_object_ref (thread);

  g_clear_signal_handler (&self->thread_changed_id, self->thread);
  g_clear_object (&self->thread);
  self->thread = thread;

  if (self->thread != NULL)
    self->thread_changed_id = g_signal_connect_object (self->thread,
                                                       "notify::stopped",
                                                       G_CALLBACK (foundry_debugger_actions_thread_changed_cb),
                                                       self,
                                                       G_CONNECT_SWAPPED);

  foundry_debugger_actions_thread_changed_cb (self, NULL, self->thread);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_THREAD]);
}
