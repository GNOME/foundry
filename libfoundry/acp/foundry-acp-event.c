/* foundry-acp-event.c
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
#include "foundry-acp-event-private.h"

struct _FoundryAcpEvent
{
  GObject parent_instance;
  char *session_id;
  char *turn_id;
  char *event_id;
  char *parent_event_id;
  char *raw_kind;
  char *title;
  char *body;
  JsonNode *raw;
  gint64 timestamp;
  FoundryAcpEventKind kind;
  FoundryAcpEventState state;
};

G_DEFINE_FINAL_TYPE (FoundryAcpEvent, foundry_acp_event, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SESSION_ID,
  PROP_TURN_ID,
  PROP_EVENT_ID,
  PROP_PARENT_EVENT_ID,
  PROP_KIND,
  PROP_RAW_KIND,
  PROP_TITLE,
  PROP_BODY,
  PROP_TIMESTAMP,
  PROP_STATE,
  PROP_RAW,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_acp_event_finalize (GObject *object)
{
  FoundryAcpEvent *self = (FoundryAcpEvent *)object;

  g_clear_pointer (&self->session_id, g_free);
  g_clear_pointer (&self->turn_id, g_free);
  g_clear_pointer (&self->event_id, g_free);
  g_clear_pointer (&self->parent_event_id, g_free);
  g_clear_pointer (&self->raw_kind, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->body, g_free);
  g_clear_pointer (&self->raw, json_node_unref);

  G_OBJECT_CLASS (foundry_acp_event_parent_class)->finalize (object);
}

static void
foundry_acp_event_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  FoundryAcpEvent *self = FOUNDRY_ACP_EVENT (object);

  switch (prop_id)
    {
    case PROP_SESSION_ID:
      g_value_set_string (value, self->session_id);
      break;

    case PROP_TURN_ID:
      g_value_set_string (value, self->turn_id);
      break;

    case PROP_EVENT_ID:
      g_value_set_string (value, self->event_id);
      break;

    case PROP_PARENT_EVENT_ID:
      g_value_set_string (value, self->parent_event_id);
      break;

    case PROP_KIND:
      g_value_set_enum (value, self->kind);
      break;

    case PROP_RAW_KIND:
      g_value_set_string (value, self->raw_kind);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_BODY:
      g_value_set_string (value, self->body);
      break;

    case PROP_TIMESTAMP:
      g_value_set_int64 (value, self->timestamp);
      break;

    case PROP_STATE:
      g_value_set_enum (value, self->state);
      break;

    case PROP_RAW:
      g_value_set_boxed (value, self->raw);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_acp_event_class_init (FoundryAcpEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_event_finalize;
  object_class->get_property = foundry_acp_event_get_property;

  properties[PROP_SESSION_ID] =
    g_param_spec_string ("session-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TURN_ID] =
    g_param_spec_string ("turn-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_EVENT_ID] =
    g_param_spec_string ("event-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_PARENT_EVENT_ID] =
    g_param_spec_string ("parent-event-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_KIND] =
    g_param_spec_enum ("kind", NULL, NULL,
                       FOUNDRY_TYPE_ACP_EVENT_KIND,
                       FOUNDRY_ACP_EVENT_UNKNOWN,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_RAW_KIND] =
    g_param_spec_string ("raw-kind", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_BODY] =
    g_param_spec_string ("body", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TIMESTAMP] =
    g_param_spec_int64 ("timestamp", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_STATE] =
    g_param_spec_enum ("state", NULL, NULL,
                       FOUNDRY_TYPE_ACP_EVENT_STATE,
                       FOUNDRY_ACP_EVENT_PENDING,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_RAW] =
    g_param_spec_boxed ("raw", NULL, NULL,
                        JSON_TYPE_NODE,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_acp_event_init (FoundryAcpEvent *self)
{
  self->timestamp = g_get_real_time ();
}

/**
 * foundry_acp_event_new:
 * @session_id: the ACP session identifier
 * @kind: the normalized event kind
 *
 * Creates a new monitoring event.
 *
 * Returns: (transfer full): a [class@Foundry.AcpEvent]
 *
 * Since: 1.2
 */
FoundryAcpEvent *
foundry_acp_event_new (const char          *session_id,
                       FoundryAcpEventKind  kind)
{
  g_return_val_if_fail (session_id != NULL, NULL);

  return _foundry_acp_event_new_full (session_id, NULL, NULL, NULL,
                                      kind, NULL, NULL, NULL,
                                      FOUNDRY_ACP_EVENT_COMPLETED, NULL);
}

FoundryAcpEvent *
_foundry_acp_event_new_full (const char           *session_id,
                             const char           *turn_id,
                             const char           *event_id,
                             const char           *parent_event_id,
                             FoundryAcpEventKind   kind,
                             const char           *raw_kind,
                             const char           *title,
                             const char           *body,
                             FoundryAcpEventState  state,
                             JsonNode             *raw)
{
  FoundryAcpEvent *self;

  g_return_val_if_fail (session_id != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_EVENT, NULL);
  self->session_id = g_strdup (session_id);
  self->turn_id = g_strdup (turn_id);
  self->event_id = g_strdup (event_id);
  self->parent_event_id = g_strdup (parent_event_id);
  self->kind = kind;
  self->raw_kind = g_strdup (raw_kind);
  self->title = g_strdup (title);
  self->body = g_strdup (body);
  self->state = state;

  if (raw != NULL)
    self->raw = json_node_ref (raw);

  return self;
}

/**
 * foundry_acp_event_dup_session_id:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: (transfer full): the ACP session identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_event_dup_session_id (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), NULL);

  return g_strdup (self->session_id);
}

/**
 * foundry_acp_event_dup_turn_id:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: (transfer full) (nullable): the prompt turn identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_event_dup_turn_id (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), NULL);

  return g_strdup (self->turn_id);
}

/**
 * foundry_acp_event_dup_event_id:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: (transfer full) (nullable): the event identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_event_dup_event_id (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), NULL);

  return g_strdup (self->event_id);
}

/**
 * foundry_acp_event_dup_parent_event_id:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: (transfer full) (nullable): the parent event identifier
 *
 * Since: 1.2
 */
char *
foundry_acp_event_dup_parent_event_id (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), NULL);

  return g_strdup (self->parent_event_id);
}

/**
 * foundry_acp_event_get_kind:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: the normalized event kind
 *
 * Since: 1.2
 */
FoundryAcpEventKind
foundry_acp_event_get_kind (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), FOUNDRY_ACP_EVENT_UNKNOWN);

  return self->kind;
}

/**
 * foundry_acp_event_dup_raw_kind:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: (transfer full) (nullable): the protocol-specific kind
 *
 * Since: 1.2
 */
char *
foundry_acp_event_dup_raw_kind (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), NULL);

  return g_strdup (self->raw_kind);
}

/**
 * foundry_acp_event_dup_title:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: (transfer full) (nullable): a short human-facing summary
 *
 * Since: 1.2
 */
char *
foundry_acp_event_dup_title (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), NULL);

  return g_strdup (self->title);
}

/**
 * foundry_acp_event_dup_body:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: (transfer full) (nullable): event detail text
 *
 * Since: 1.2
 */
char *
foundry_acp_event_dup_body (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), NULL);

  return g_strdup (self->body);
}

/**
 * foundry_acp_event_get_timestamp:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: a wall-clock timestamp in microseconds since the Unix epoch
 *
 * Since: 1.2
 */
gint64
foundry_acp_event_get_timestamp (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), 0);

  return self->timestamp;
}

/**
 * foundry_acp_event_get_state:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: the event state
 *
 * Since: 1.2
 */
FoundryAcpEventState
foundry_acp_event_get_state (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), FOUNDRY_ACP_EVENT_FAILED);

  return self->state;
}

/**
 * foundry_acp_event_dup_raw:
 * @self: a [class@Foundry.AcpEvent]
 *
 * Returns: (transfer full) (nullable): the raw protocol JSON for diagnostics
 *
 * Since: 1.2
 */
JsonNode *
foundry_acp_event_dup_raw (FoundryAcpEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_EVENT (self), NULL);

  if (self->raw == NULL)
    return NULL;

  return json_node_ref (self->raw);
}

void
_foundry_acp_event_set_turn_id (FoundryAcpEvent *self,
                                const char      *turn_id)
{
  g_return_if_fail (FOUNDRY_IS_ACP_EVENT (self));

  if (g_set_str (&self->turn_id, turn_id))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TURN_ID]);
}

void
_foundry_acp_event_set_raw_kind (FoundryAcpEvent *self,
                                 const char      *raw_kind)
{
  g_return_if_fail (FOUNDRY_IS_ACP_EVENT (self));

  if (g_set_str (&self->raw_kind, raw_kind))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RAW_KIND]);
}

void
_foundry_acp_event_set_title (FoundryAcpEvent *self,
                              const char      *title)
{
  g_return_if_fail (FOUNDRY_IS_ACP_EVENT (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

void
_foundry_acp_event_set_body (FoundryAcpEvent *self,
                             const char      *body)
{
  g_return_if_fail (FOUNDRY_IS_ACP_EVENT (self));

  if (g_set_str (&self->body, body))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BODY]);
}

void
_foundry_acp_event_set_state (FoundryAcpEvent     *self,
                              FoundryAcpEventState state)
{
  g_return_if_fail (FOUNDRY_IS_ACP_EVENT (self));

  if (self->state != state)
    {
      self->state = state;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
    }
}

void
_foundry_acp_event_set_raw (FoundryAcpEvent *self,
                            JsonNode        *raw)
{
  g_return_if_fail (FOUNDRY_IS_ACP_EVENT (self));

  if (raw != NULL)
    json_node_ref (raw);

  g_clear_pointer (&self->raw, json_node_unref);
  self->raw = raw;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RAW]);
}
