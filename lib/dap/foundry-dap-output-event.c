/* foundry-output-event.c
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
#include "foundry-dap-output-event.h"

struct _FoundryDapOutputEvent
{
  FoundryDapEvent parent_instance;
};

struct _FoundryDapOutputEventClass
{
  FoundryDapEventClass parent_class;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryDapOutputEvent, foundry_dap_output_event, FOUNDRY_TYPE_DAP_EVENT)

static GParamSpec *properties[N_PROPS];

static void
foundry_dap_output_event_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_dap_output_event_class_init (FoundryDapOutputEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_dap_output_event_get_property;
}

static void
foundry_dap_output_event_init (FoundryDapOutputEvent *self)
{
}

const char *
foundry_dap_output_event_get_category (FoundryDapOutputEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_OUTPUT_EVENT (self), NULL);

  return foundry_dap_event_get_body_member_string (FOUNDRY_DAP_EVENT (self), "category");
}

const char *
foundry_dap_output_event_get_output (FoundryDapOutputEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_OUTPUT_EVENT (self), NULL);

  return foundry_dap_event_get_body_member_string (FOUNDRY_DAP_EVENT (self), "output");
}

const char *
foundry_dap_output_event_get_group (FoundryDapOutputEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_OUTPUT_EVENT (self), NULL);

  return foundry_dap_event_get_body_member_string (FOUNDRY_DAP_EVENT (self), "group");
}

int
foundry_dap_output_event_get_location_reference (FoundryDapOutputEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_OUTPUT_EVENT (self), 0);

  return foundry_dap_event_get_body_member_int (FOUNDRY_DAP_EVENT (self), "locationReference");
}

int
foundry_dap_output_event_get_variables_reference (FoundryDapOutputEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_OUTPUT_EVENT (self), 0);

  return foundry_dap_event_get_body_member_int (FOUNDRY_DAP_EVENT (self), "variablesReference");
}

int
foundry_dap_output_event_get_line (FoundryDapOutputEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_OUTPUT_EVENT (self), 0);

  return foundry_dap_event_get_body_member_int (FOUNDRY_DAP_EVENT (self), "line");
}

int
foundry_dap_output_event_get_column (FoundryDapOutputEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_OUTPUT_EVENT (self), 0);

  return foundry_dap_event_get_body_member_int (FOUNDRY_DAP_EVENT (self), "column");
}

JsonNode *
foundry_dap_output_event_get_data (FoundryDapOutputEvent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DAP_OUTPUT_EVENT (self), NULL);

  return foundry_dap_event_get_body_member (FOUNDRY_DAP_EVENT (self), "data");
}
