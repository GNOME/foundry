/* foundry-dap-event.c
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

#include "foundry-dap-event-private.h"

G_DEFINE_ABSTRACT_TYPE (FoundryDapEvent, foundry_dap_event, FOUNDRY_TYPE_DAP_PROTOCOL_MESSAGE)

enum {
  PROP_0,
  PROP_BODY,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
foundry_dap_event_deserialize (FoundryDapProtocolMessage  *message,
                               JsonObject                 *object,
                               GError                    **error)
{
  FoundryDapEvent *self = FOUNDRY_DAP_EVENT (message);
  JsonNode *node;

  if ((node = json_object_get_member (object, "body")))
    self->body = json_node_ref (node);

  return FOUNDRY_DAP_PROTOCOL_MESSAGE_CLASS (foundry_dap_event_parent_class)->deserialize (message, object, error);
}

static void
foundry_dap_event_finalize (GObject *object)
{
  FoundryDapEvent *self = (FoundryDapEvent *)object;

  g_clear_pointer (&self->body, json_node_unref);

  G_OBJECT_CLASS (foundry_dap_event_parent_class)->finalize (object);
}

static void
foundry_dap_event_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  FoundryDapEvent *self = FOUNDRY_DAP_EVENT (object);

  switch (prop_id)
    {
    case PROP_BODY:
      g_value_set_boxed (value, self->body);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_dap_event_class_init (FoundryDapEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDapProtocolMessageClass *message_class = FOUNDRY_DAP_PROTOCOL_MESSAGE_CLASS (klass);

  object_class->finalize = foundry_dap_event_finalize;
  object_class->get_property = foundry_dap_event_get_property;

  message_class->deserialize = foundry_dap_event_deserialize;

  properties[PROP_BODY] =
    g_param_spec_boxed ("body", NULL, NULL,
                        JSON_TYPE_NODE,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_dap_event_init (FoundryDapEvent *self)
{
}

/**
 * foundry_dap_event_get_body:
 * @self: a [class@Foundry.DapEvent]
 *
 * Returns: (transfer none) (nullable):
 */
JsonNode *
foundry_dap_event_get_body (FoundryDapEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_EVENT (self), NULL);

  return self->body;
}
