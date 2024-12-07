/* foundry-log-message.c
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

#include "foundry-log-message.h"

struct _FoundryLogMessage
{
  GObject parent_instance;
};

enum {
  PROP_0,
  PROP_DOMAIN,
  PROP_MESSAGE,
  PROP_SEVERITY,
  PROP_TIME,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryLogMessage, foundry_log_message, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_log_message_finalize (GObject *object)
{
  G_OBJECT_CLASS (foundry_log_message_parent_class)->finalize (object);
}

static void
foundry_log_message_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryLogMessage *self = FOUNDRY_LOG_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_DOMAIN:
      g_value_take_string (value, foundry_log_message_dup_domain (self));
      break;

    case PROP_MESSAGE:
      g_value_take_string (value, foundry_log_message_dup_message (self));
      break;

    case PROP_SEVERITY:
      g_value_set_uint (value, foundry_log_message_get_severity (self));
      break;

    case PROP_TIME:
      g_value_take_boxed (value, foundry_log_message_dup_time (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_log_message_class_init (FoundryLogMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_log_message_finalize;
  object_class->get_property = foundry_log_message_get_property;

  properties[PROP_DOMAIN] =
    g_param_spec_string ("domain", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_MESSAGE] =
    g_param_spec_string ("message", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SEVERITY] =
    g_param_spec_uint ("severity", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_TIME] =
    g_param_spec_boxed ("time", NULL, NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_log_message_init (FoundryLogMessage *self)
{
}

/**
 * foundry_log_message_dup_domain:
 * @self: a #FoundryLogMessage
 *
 * Returns: (transfer full): the log domain
 */
char *
foundry_log_message_dup_domain (FoundryLogMessage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LOG_MESSAGE (self), NULL);

  return NULL;
}

/**
 * foundry_log_message_dup_message:
 * @self: a #FoundryLogMessage
 *
 * Returns: (transfer full): the log message
 */
char *
foundry_log_message_dup_message (FoundryLogMessage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LOG_MESSAGE (self), NULL);

  return NULL;
}

GLogLevelFlags
foundry_log_message_get_severity (FoundryLogMessage *self)
{
  return G_LOG_LEVEL_MESSAGE;
}

/**
 * foundry_log_message_dup_time:
 * @self: a #FoundryLogMessage
 *
 * Returns: (transfer full): a #GDateTime
 */
GDateTime *
foundry_log_message_dup_time (FoundryLogMessage *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LOG_MESSAGE (self), NULL);

  return NULL;
}
