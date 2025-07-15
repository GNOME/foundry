/* foundry-debugger.c
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

#include "foundry-debugger.h"
#include "foundry-debugger-mapped-region.h"
#include "foundry-debugger-target.h"
#include "foundry-debugger-trap.h"

G_DEFINE_ABSTRACT_TYPE (FoundryDebugger, foundry_debugger, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_ADDRESS_SPACE,
  PROP_TRAPS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_debugger_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  FoundryDebugger *self = FOUNDRY_DEBUGGER (object);

  switch (prop_id)
    {
    case PROP_ADDRESS_SPACE:
      g_value_take_object (value, foundry_debugger_list_address_space (self));
      break;

    case PROP_TRAPS:
      g_value_take_object (value, foundry_debugger_list_traps (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_debugger_class_init (FoundryDebuggerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_debugger_get_property;

  properties[PROP_ADDRESS_SPACE] =
    g_param_spec_object ("address-space", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TRAPS] =
    g_param_spec_object ("traps", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_debugger_init (FoundryDebugger *self)
{
}

/**
 * foundry_debugger_dup_name:
 * @self: a #FoundryDebugger
 *
 * Gets a name for the provider that is expected to be displayed to
 * users such as "GNU Debugger".
 *
 * Returns: (transfer full): the name of the provider
 */
char *
foundry_debugger_dup_name (FoundryDebugger *self)
{
  char *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER (self), NULL);

  if (FOUNDRY_DEBUGGER_GET_CLASS (self)->dup_name)
    ret = FOUNDRY_DEBUGGER_GET_CLASS (self)->dup_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return g_steal_pointer (&ret);
}

/**
 * foundry_debugger_connect_to_target:
 * @self: a [class@Foundry.Debugger]
 * @target: a [class@Foundry.DebuggerTarget]
 *
 * Connects to @target.
 *
 * Not all debuggers may not support all debugger target types.
 *
 * Returns: (transfer full): [class@Dex.Future] that resolves to any value
 *   or rejects with error.
 */
DexFuture *
foundry_debugger_connect_to_target (FoundryDebugger       *self,
                                    FoundryDebuggerTarget *target)
{
  dex_return_error_if_fail (FOUNDRY_IS_DEBUGGER (self));
  dex_return_error_if_fail (FOUNDRY_IS_DEBUGGER_TARGET (target));

  return FOUNDRY_DEBUGGER_GET_CLASS (self)->connect_to_target (self, target);
}

/**
 * foundry_debugger_initialize:
 * @self: a [class@Foundry.Debugger]
 *
 * This must be called before using the debugger instance and may only
 * be called once.
 *
 * Subclasses are expected to perform capability negotiation as part
 * of this request.
 *
 * Returns: (transfer full): [class@Dex.Future] that resolves to any value
 *   or rejects with error.
 */
DexFuture *
foundry_debugger_initialize (FoundryDebugger *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_DEBUGGER (self));

  return FOUNDRY_DEBUGGER_GET_CLASS (self)->initialize (self);
}

/**
 * foundry_debugger_list_address_space:
 * @self: a [class@Foundry.Debugger]
 *
 * Gets a [iface@Gio.ListModel] of [class@Foundry.DebuggerMappedRegion]
 * that is updated based on the address mapping of the debuggee.
 *
 * Returns: (transfer full):
 */
GListModel *
foundry_debugger_list_address_space (FoundryDebugger *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER (self), NULL);

  if (FOUNDRY_DEBUGGER_GET_CLASS (self)->list_address_space)
    return FOUNDRY_DEBUGGER_GET_CLASS (self)->list_address_space (self);

  return G_LIST_MODEL (g_list_store_new (FOUNDRY_TYPE_DEBUGGER_MAPPED_REGION));
}

/**
 * foundry_debugger_list_traps:
 * @self: a [class@Foundry.Debugger]
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of [class@Foundry.DebuggerTrap]
 */
GListModel *
foundry_debugger_list_traps (FoundryDebugger *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEBUGGER (self), NULL);

  if (FOUNDRY_DEBUGGER_GET_CLASS (self)->list_traps)
    return FOUNDRY_DEBUGGER_GET_CLASS (self)->list_traps (self);

  return G_LIST_MODEL (g_list_store_new (FOUNDRY_TYPE_DEBUGGER_TRAP));
}
