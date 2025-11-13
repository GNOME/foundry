/* foundry-terminal-intent.c
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

#include "foundry-terminal-intent.h"
#include "foundry-terminal-launcher.h"

struct _FoundryTerminalIntent
{
  FoundryIntent            parent_instance;
  FoundryTerminalLauncher *launcher;
};

G_DEFINE_FINAL_TYPE (FoundryTerminalIntent, foundry_terminal_intent, FOUNDRY_TYPE_INTENT)

enum {
  PROP_0,
  PROP_LAUNCHER,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_terminal_intent_finalize (GObject *object)
{
  FoundryTerminalIntent *self = (FoundryTerminalIntent *)object;

  g_clear_object (&self->launcher);

  G_OBJECT_CLASS (foundry_terminal_intent_parent_class)->finalize (object);
}

static void
foundry_terminal_intent_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  FoundryTerminalIntent *self = FOUNDRY_TERMINAL_INTENT (object);

  switch (prop_id)
    {
    case PROP_LAUNCHER:
      g_value_take_object (value, foundry_terminal_intent_dup_launcher (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_terminal_intent_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  FoundryTerminalIntent *self = FOUNDRY_TERMINAL_INTENT (object);

  switch (prop_id)
    {
    case PROP_LAUNCHER:
      self->launcher = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_terminal_intent_class_init (FoundryTerminalIntentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_terminal_intent_finalize;
  object_class->get_property = foundry_terminal_intent_get_property;
  object_class->set_property = foundry_terminal_intent_set_property;

  properties[PROP_LAUNCHER] =
    g_param_spec_object ("launcher", NULL, NULL,
                         FOUNDRY_TYPE_TERMINAL_LAUNCHER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_terminal_intent_init (FoundryTerminalIntent *self)
{
}

/**
 * foundry_terminal_intent_new:
 * @launcher: a [class@Foundry.TerminalLauncher]
 *
 * Create a new terminal intent.
 *
 * Returns: (transfer full):
 *
 * Since: 1.1
 */
FoundryIntent *
foundry_terminal_intent_new (FoundryTerminalLauncher *launcher)
{
  g_return_val_if_fail (FOUNDRY_IS_TERMINAL_LAUNCHER (launcher), NULL);

  return g_object_new (FOUNDRY_TYPE_TERMINAL_INTENT,
                       "launcher", launcher,
                       NULL);
}

/**
 * foundry_terminal_intent_dup_launcher:
 * @self: a [class@Foundry.TerminalIntent]
 *
 * Returns: (transfer full) (not nullable):
 *
 * Since: 1.1
 */
FoundryTerminalLauncher *
foundry_terminal_intent_dup_launcher (FoundryTerminalIntent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TERMINAL_INTENT (self), NULL);

  return g_object_ref (self->launcher);
}
