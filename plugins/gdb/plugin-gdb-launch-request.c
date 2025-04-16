/* plugin-gdb-launch-request.c
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

#include "plugin-gdb-launch-request.h"

struct _PluginGdbLaunchRequest
{
  FoundryDapRequest parent_instance;
  JsonObject *object;
};

G_DEFINE_FINAL_TYPE (PluginGdbLaunchRequest, plugin_gdb_launch_request, FOUNDRY_TYPE_DAP_REQUEST)

static gboolean
plugin_gdb_launch_request_serialize (FoundryDapProtocolMessage  *message,
                                     JsonObject                 *object,
                                     GError                    **error)
{
  PluginGdbLaunchRequest *self = PLUGIN_GDB_LAUNCH_REQUEST (message);

  json_object_set_string_member (object, "command", "launch");
  json_object_set_object_member (object, "arguments", json_object_ref (self->object));

  return TRUE;
}

static void
plugin_gdb_launch_request_finalize (GObject *object)
{
  PluginGdbLaunchRequest *self = (PluginGdbLaunchRequest *)object;

  g_clear_pointer (&self->object, json_object_unref);

  G_OBJECT_CLASS (plugin_gdb_launch_request_parent_class)->finalize (object);
}

static void
plugin_gdb_launch_request_class_init (PluginGdbLaunchRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDapProtocolMessageClass *protocol_message_class = FOUNDRY_DAP_PROTOCOL_MESSAGE_CLASS (klass);

  object_class->finalize = plugin_gdb_launch_request_finalize;

  protocol_message_class->serialize = plugin_gdb_launch_request_serialize;
}

static void
plugin_gdb_launch_request_init (PluginGdbLaunchRequest *self)
{
}

FoundryDapRequest *
plugin_gdb_launch_request_new (const char * const *args,
                               const char         *cwd,
                               const char * const *env,
                               const char         *program,
                               gboolean            stop_at_main,
                               gboolean            stop_on_entry)
{
  PluginGdbLaunchRequest *self;

  self = g_object_new (PLUGIN_TYPE_GDB_LAUNCH_REQUEST, NULL);
  self->object = json_object_new ();

  if (args)
    json_object_set_member (self->object, "args", foundry_json_node_new_strv (args));

  if (env)
    json_object_set_member (self->object, "env", foundry_json_node_new_strv (args));

  if (cwd)
    json_object_set_string_member (self->object, "cwd", cwd);

  if (program)
    json_object_set_string_member (self->object, "program", program);

  if (stop_at_main)
    json_object_set_boolean_member (self->object, "stopAtBeginningOfMainSubprogram", TRUE);

  if (stop_on_entry)
    json_object_set_boolean_member (self->object, "stopOnEntry", TRUE);

  return FOUNDRY_DAP_REQUEST (self);
}
