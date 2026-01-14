/* foundry-team.c
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-team.h"
#include "foundry-team-artifact-file.h"
#include "foundry-team-artifact-diff.h"
#include "foundry-team-persona.h"
#include "foundry-team-progress-private.h"
#include "foundry-util-private.h"

/**
 * FoundryTeam:
 *
 * Orchestrates a team of personas working in a directed graph workflow.
 */

struct _FoundryTeam
{
  FoundryContextual  parent_instance;
  GListStore        *personas;
};

enum {
  PROP_0,
  PROP_PERSONAS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (FoundryTeam, foundry_team, FOUNDRY_TYPE_CONTEXTUAL)

static void
foundry_team_dispose (GObject *object)
{
  FoundryTeam *self = FOUNDRY_TEAM (object);

  g_clear_object (&self->personas);

  G_OBJECT_CLASS (foundry_team_parent_class)->dispose (object);
}

static void
foundry_team_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  FoundryTeam *self = FOUNDRY_TEAM (object);

  switch (prop_id)
    {
    case PROP_PERSONAS:
      g_value_take_object (value, foundry_team_list_personas (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_team_class_init (FoundryTeamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_team_dispose;
  object_class->get_property = foundry_team_get_property;

  properties[PROP_PERSONAS] =
    g_param_spec_object ("personas", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_team_init (FoundryTeam *self)
{
  self->personas = g_list_store_new (FOUNDRY_TYPE_TEAM_PERSONA);
}

FoundryTeam *
foundry_team_new (void)
{
  return g_object_new (FOUNDRY_TYPE_TEAM, NULL);
}

/**
 * foundry_team_list_personas:
 * @self: a [class@Foundry.Team]
 *
 * Returns: (transfer full):
 */
GListModel *
foundry_team_list_personas (FoundryTeam *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEAM (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->personas));
}

void
foundry_team_add_persona (FoundryTeam        *self,
                          FoundryTeamPersona *persona)
{
  g_return_if_fail (FOUNDRY_IS_TEAM (self));
  g_return_if_fail (FOUNDRY_IS_TEAM_PERSONA (persona));

  g_list_store_append (self->personas, persona);
}

void
foundry_team_remove_persona (FoundryTeam        *self,
                             FoundryTeamPersona *persona)
{
  guint n_items;

  g_return_if_fail (FOUNDRY_IS_TEAM (self));
  g_return_if_fail (FOUNDRY_IS_TEAM_PERSONA (persona));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->personas));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTeamPersona) element = g_list_model_get_item (G_LIST_MODEL (self->personas), i);

      if (element == persona)
        {
          g_list_store_remove (self->personas, i);
          break;
        }
    }
}

/**
 * foundry_team_execute:
 * @self: a [class@Foundry.Team]
 *
 * Returns: (transfer full):
 */
FoundryTeamProgress *
foundry_team_execute (FoundryTeam *self,
                      GListModel  *context,
                      GListModel  *artifacts)
{
  g_autoptr(FoundryContext) f_context = NULL;

  g_return_val_if_fail (FOUNDRY_IS_TEAM (self), NULL);
  g_return_val_if_fail (!context || G_IS_LIST_MODEL (context), NULL);
  g_return_val_if_fail (!artifacts || G_IS_LIST_MODEL (artifacts), NULL);

  if (!(f_context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    return NULL;

  return _foundry_team_progress_new (f_context, self);
}
