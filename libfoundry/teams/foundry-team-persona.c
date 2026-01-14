/* foundry-team-persona.c
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

#include "foundry-team-persona-private.h"
#include "foundry-util-private.h"

/**
 * FoundryTeamPersona:
 *
 * Represents a persona (agent role) in a team workflow.
 *
 * FoundryTeamPersona defines a role (e.g., "planner", "coder", "qa", "linter")
 * and a project-controlled prompt that defines how the persona behaves.
 * Personas are arranged in a directed graph within a FoundryTeam and can
 * communicate by passing artifacts between each other.
 */

struct _FoundryTeamPersona
{
  GObject  parent_instance;
  char    *role;
  char    *prompt;
};

enum {
  PROP_0,
  PROP_PROMPT,
  PROP_ROLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (FoundryTeamPersona, foundry_team_persona, G_TYPE_OBJECT)

static void
foundry_team_persona_finalize (GObject *object)
{
  FoundryTeamPersona *self = FOUNDRY_TEAM_PERSONA (object);

  g_clear_pointer (&self->role, g_free);
  g_clear_pointer (&self->prompt, g_free);

  G_OBJECT_CLASS (foundry_team_persona_parent_class)->finalize (object);
}

static void
foundry_team_persona_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryTeamPersona *self = FOUNDRY_TEAM_PERSONA (object);

  switch (prop_id)
    {
    case PROP_PROMPT:
      g_value_take_string (value, foundry_team_persona_dup_prompt (self));
      break;

    case PROP_ROLE:
      g_value_take_string (value, foundry_team_persona_dup_role (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_team_persona_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  FoundryTeamPersona *self = FOUNDRY_TEAM_PERSONA (object);

  switch (prop_id)
    {
    case PROP_PROMPT:
      g_set_str (&self->prompt, g_value_get_string (value));
      break;

    case PROP_ROLE:
      g_set_str (&self->role, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_team_persona_class_init (FoundryTeamPersonaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_team_persona_finalize;
  object_class->get_property = foundry_team_persona_get_property;
  object_class->set_property = foundry_team_persona_set_property;

  properties[PROP_PROMPT] =
    g_param_spec_string ("prompt", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ROLE] =
    g_param_spec_string ("role", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_team_persona_init (FoundryTeamPersona *self)
{
}

FoundryTeamPersona *
foundry_team_persona_new (const char *role,
                          const char *prompt)
{
  return g_object_new (FOUNDRY_TYPE_TEAM_PERSONA,
                       "role", role,
                       "prompt", prompt,
                       NULL);
}

char *
foundry_team_persona_dup_role (FoundryTeamPersona *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEAM_PERSONA (self), NULL);

  return g_strdup (self->role);
}

char *
foundry_team_persona_dup_prompt (FoundryTeamPersona *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEAM_PERSONA (self), NULL);

  return g_strdup (self->prompt);
}

DexFuture *
_foundry_team_persona_run (FoundryTeamPersona *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEAM_PERSONA (self));

  return foundry_future_new_not_supported ();
}
