/* plugin-gdb-attach-request.c
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

#include "plugin-gdb-attach-request.h"

struct _PluginGdbAttachRequest
{
  FoundryDapRequest parent_instance;
  JsonNode *node;
};

G_DEFINE_FINAL_TYPE (PluginGdbAttachRequest, plugin_gdb_attach_request, FOUNDRY_TYPE_DAP_REQUEST)

static void
plugin_gdb_attach_request_finalize (GObject *object)
{
  PluginGdbAttachRequest *self = (PluginGdbAttachRequest *)object;

  g_clear_pointer (&self->node, json_node_unref);

  G_OBJECT_CLASS (plugin_gdb_attach_request_parent_class)->finalize (object);
}

static void
plugin_gdb_attach_request_class_init (PluginGdbAttachRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_gdb_attach_request_finalize;
}

static void
plugin_gdb_attach_request_init (PluginGdbAttachRequest *self)
{
}

FoundryDapRequest *
plugin_gdb_attach_request_new_local (GPid        pid,
                                     const char *program)
{
  PluginGdbAttachRequest *self;
  g_autoptr(JsonObject) object = NULL;

  self = g_object_new (PLUGIN_TYPE_GDB_ATTACH_REQUEST, NULL);
  self->node = json_node_new (JSON_NODE_OBJECT);

  json_object_set_int_member (object, "pid", pid);

  if (program)
    json_object_set_string_member (object, "program", program);

  json_node_set_object (self->node, object);

  return FOUNDRY_DAP_REQUEST (self);
}

FoundryDapRequest *
plugin_gdb_attach_request_new_remote (const char *address,
                                      const char *program)
{
  PluginGdbAttachRequest *self;
  g_autoptr(JsonObject) object = NULL;

  self = g_object_new (PLUGIN_TYPE_GDB_ATTACH_REQUEST, NULL);
  self->node = json_node_new (JSON_NODE_OBJECT);

  json_object_set_string_member (object, "target", address);

  if (program)
    json_object_set_string_member (object, "program", program);

  json_node_set_object (self->node, object);

  return FOUNDRY_DAP_REQUEST (self);
}
