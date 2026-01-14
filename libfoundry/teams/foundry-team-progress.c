/* foundry-team-progress.c
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
#include "foundry-team-progress-private.h"
#include "foundry-team-progress.h"
#include "foundry-util-private.h"

/**
 * FoundryTeamProgress:
 *
 * Tracks the execution progress of a team workflow.
 *
 * FoundryTeamProgress provides functionality for tracking the execution
 * of a FoundryTeam workflow, including persona traversal and artifact
 * management.
 *
 * Since: 1.1
 */

struct _FoundryTeamProgress
{
  FoundryContextual  parent_instance;
  DexPromise        *completion;
};

G_DEFINE_FINAL_TYPE (FoundryTeamProgress, foundry_team_progress, FOUNDRY_TYPE_CONTEXTUAL)

static void
foundry_team_progress_dispose (GObject *object)
{
  FoundryTeamProgress *self = FOUNDRY_TEAM_PROGRESS (object);

  if (dex_future_is_pending (DEX_FUTURE (self->completion)))
    dex_promise_reject (self->completion,
                        g_error_new (G_IO_ERROR,
                                     G_IO_ERROR_CANCELLED,
                                     "Team progress cancelled"));

  G_OBJECT_CLASS (foundry_team_progress_parent_class)->dispose (object);
}

static void
foundry_team_progress_finalize (GObject *object)
{
  FoundryTeamProgress *self = FOUNDRY_TEAM_PROGRESS (object);

  dex_clear (&self->completion);

  G_OBJECT_CLASS (foundry_team_progress_parent_class)->finalize (object);
}

static void
foundry_team_progress_class_init (FoundryTeamProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_team_progress_dispose;
  object_class->finalize = foundry_team_progress_finalize;
}

static void
foundry_team_progress_init (FoundryTeamProgress *self)
{
  self->completion = dex_promise_new ();
}

typedef struct _Run
{
  GWeakRef   self_wr;
  GPtrArray *personas;
} Run;

static void
run_free (Run *state)
{
  g_weak_ref_clear (&state->self_wr);
  g_clear_pointer (&state->personas, g_ptr_array_unref);
  g_free (state);
}

static DexFuture *
foundry_team_progress_fiber (gpointer user_data)
{
  g_autoptr(FoundryTeamProgress) self = NULL;
  Run *state = user_data;

  g_assert (state != NULL);
  g_assert (state->personas != NULL);

  if (!(self = g_weak_ref_get (&state->self_wr)))
    return foundry_future_new_disposed ();

  for (int i = 0; i < state->personas->len; i++)
    {
      FoundryTeamPersona *persona = g_ptr_array_index (state->personas, i);
      g_autoptr(GError) error = NULL;

      /* TODO: determine what is going to happen (continue or pop back
       *       to the previous persona).
       */
      if (!dex_await (_foundry_team_persona_run (persona), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return NULL;
}

FoundryTeamProgress *
_foundry_team_progress_new (FoundryContext *context,
                            FoundryTeam    *team)
{
  FoundryTeamProgress *self;
  Run *state;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FOUNDRY_IS_TEAM (team), NULL);

  self = g_object_new (FOUNDRY_TYPE_TEAM_PROGRESS,
                       "context", context,
                       NULL);

  state = g_new0 (Run, 1);
  g_weak_ref_init (&state->self_wr, self);
  state->personas = g_ptr_array_new_with_free_func (g_object_unref);

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          foundry_team_progress_fiber,
                                          state,
                                          (GDestroyNotify) run_free));

  return self;
}

/**
 * foundry_team_progress_await:
 * @self: a [class@Foundry.TeamProgress]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value when the progress has completed.
 *
 * Since: 1.1
 */
DexFuture *
foundry_team_progress_await (FoundryTeamProgress *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEAM_PROGRESS (self), NULL);

  return dex_ref (self->completion);
}
