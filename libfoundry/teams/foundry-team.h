/* foundry-team.h
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

#pragma once

#include <libdex.h>

#include "foundry-contextual.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_TEAM (foundry_team_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryTeam, foundry_team, FOUNDRY, TEAM, FoundryContextual)

FOUNDRY_AVAILABLE_IN_1_1
FoundryTeam         *foundry_team_new            (void);
FOUNDRY_AVAILABLE_IN_1_1
GListModel          *foundry_team_list_personas  (FoundryTeam        *self);
FOUNDRY_AVAILABLE_IN_1_1
void                 foundry_team_add_persona    (FoundryTeam        *self,
                                                  FoundryTeamPersona *persona);
FOUNDRY_AVAILABLE_IN_1_1
void                 foundry_team_remove_persona (FoundryTeam        *self,
                                                  FoundryTeamPersona *persona);
FOUNDRY_AVAILABLE_IN_1_1
FoundryTeamProgress *foundry_team_execute        (FoundryTeam        *self,
                                                  GListModel         *context,
                                                  GListModel         *artifacts) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
