/* plugin-ctags-completion-proposal.c
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

#include "plugin-ctags-completion-proposal.h"

struct _PluginCtagsCompletionProposal
{
  FoundryCompletionProposal parent_instance;
  PluginCtagsFile *file;
  guint position;
};

G_DEFINE_FINAL_TYPE (PluginCtagsCompletionProposal, plugin_ctags_completion_proposal, FOUNDRY_TYPE_COMPLETION_PROPOSAL)

static char *
plugin_ctags_completion_proposal_dup_typed_text (FoundryCompletionProposal *proposal)
{
  PluginCtagsCompletionProposal *self = PLUGIN_CTAGS_COMPLETION_PROPOSAL (proposal);

  return plugin_ctags_file_dup_name (self->file, self->position);
}

static void
plugin_ctags_completion_proposal_finalize (GObject *object)
{
  PluginCtagsCompletionProposal *self = (PluginCtagsCompletionProposal *)object;

  g_clear_object (&self->file);

  G_OBJECT_CLASS (plugin_ctags_completion_proposal_parent_class)->finalize (object);
}

static void
plugin_ctags_completion_proposal_class_init (PluginCtagsCompletionProposalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCompletionProposalClass *proposal_class = FOUNDRY_COMPLETION_PROPOSAL_CLASS (klass);

  object_class->finalize = plugin_ctags_completion_proposal_finalize;

  proposal_class->dup_typed_text = plugin_ctags_completion_proposal_dup_typed_text;
}

static void
plugin_ctags_completion_proposal_init (PluginCtagsCompletionProposal *self)
{
}

PluginCtagsCompletionProposal *
plugin_ctags_completion_proposal_new (PluginCtagsFile *file,
                                      guint            position)
{
  PluginCtagsCompletionProposal *self;

  self = g_object_new (PLUGIN_TYPE_CTAGS_COMPLETION_PROPOSAL, NULL);
  self->file = g_object_ref (file);
  self->position = position;

  return self;
}
