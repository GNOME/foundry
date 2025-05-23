/* foundry-lsp-completion-proposal.c
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

#include <jsonrpc-glib.h>

#include "foundry-lsp-completion-proposal-private.h"

enum {
	LSP_COMPLETION_TEXT           = 1,
	LSP_COMPLETION_METHOD         = 2,
	LSP_COMPLETION_FUNCTION       = 3,
	LSP_COMPLETION_CONSTRUCTOR    = 4,
	LSP_COMPLETION_FIELD          = 5,
	LSP_COMPLETION_VARIABLE       = 6,
	LSP_COMPLETION_CLASS          = 7,
	LSP_COMPLETION_INTERFACE      = 8,
	LSP_COMPLETION_MODULE         = 9,
	LSP_COMPLETION_PROPERTY       = 10,
	LSP_COMPLETION_UNIT           = 11,
	LSP_COMPLETION_VALUE          = 12,
	LSP_COMPLETION_ENUM           = 13,
	LSP_COMPLETION_KEYWORD        = 14,
	LSP_COMPLETION_SNIPPET        = 15,
	LSP_COMPLETION_COLOR          = 16,
	LSP_COMPLETION_FILE           = 17,
	LSP_COMPLETION_REFERENCE      = 18,
	LSP_COMPLETION_FOLDER         = 19,
	LSP_COMPLETION_ENUM_MEMBER    = 20,
	LSP_COMPLETION_CONSTANT       = 21,
	LSP_COMPLETION_STRUCT         = 22,
	LSP_COMPLETION_EVENT          = 23,
	LSP_COMPLETION_OPERATOR       = 24,
	LSP_COMPLETION_TYPE_PARAMETER = 25,
};

struct _FoundryLspCompletionProposal
{
  FoundryCompletionProposal  parent_instance;
  GVariant                  *info;
  const char                *label;
  const char                *detail;
  guint                      kind;
};

G_DEFINE_FINAL_TYPE (FoundryLspCompletionProposal, foundry_lsp_completion_proposal, FOUNDRY_TYPE_COMPLETION_PROPOSAL)

static char *
foundry_lsp_completion_proposal_dup_typed_text (FoundryCompletionProposal *proposal)
{
  return g_strdup (FOUNDRY_LSP_COMPLETION_PROPOSAL (proposal)->label);
}

static GIcon *
foundry_lsp_completion_proposal_dup_icon (FoundryCompletionProposal *proposal)
{
  FoundryLspCompletionProposal *self = FOUNDRY_LSP_COMPLETION_PROPOSAL (proposal);

  switch (self->kind)
    {
    case LSP_COMPLETION_METHOD:
      return g_themed_icon_new ("lang-method-symbolic");

    case LSP_COMPLETION_CONSTRUCTOR:
    case LSP_COMPLETION_FUNCTION:
      return g_themed_icon_new ("lang-function-symbolic");

    case LSP_COMPLETION_VARIABLE:
      return g_themed_icon_new ("lang-struct-field-symbolic");

    case LSP_COMPLETION_CLASS:
      return g_themed_icon_new ("lang-class-symbolic");

    case LSP_COMPLETION_PROPERTY:
      return g_themed_icon_new ("lang-property-symbolic");

    case LSP_COMPLETION_ENUM:
      return g_themed_icon_new ("lang-enum-symbolic");

    case LSP_COMPLETION_ENUM_MEMBER:
      return g_themed_icon_new ("lang-constant-symbolic");

    case LSP_COMPLETION_STRUCT:
      return g_themed_icon_new ("lang-struct-symbolic");

    default:
      break;
    }

  return NULL;
}

static void
foundry_lsp_completion_proposal_finalize (GObject *object)
{
  FoundryLspCompletionProposal *self = (FoundryLspCompletionProposal *)object;

  g_clear_pointer (&self->info, g_variant_unref);

  self->label = NULL;
  self->detail = NULL;

  G_OBJECT_CLASS (foundry_lsp_completion_proposal_parent_class)->finalize (object);
}

static void
foundry_lsp_completion_proposal_class_init (FoundryLspCompletionProposalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryCompletionProposalClass *proposal_class = FOUNDRY_COMPLETION_PROPOSAL_CLASS (klass);

  object_class->finalize = foundry_lsp_completion_proposal_finalize;

  proposal_class->dup_icon = foundry_lsp_completion_proposal_dup_icon;
  proposal_class->dup_typed_text = foundry_lsp_completion_proposal_dup_typed_text;
}

static void
foundry_lsp_completion_proposal_init (FoundryLspCompletionProposal *self)
{
}

FoundryLspCompletionProposal *
_foundry_lsp_completion_proposal_new (GVariant *info)
{
  FoundryLspCompletionProposal *self;
  gint64 kind;

  g_return_val_if_fail (info != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_LSP_COMPLETION_PROPOSAL, NULL);

  if (g_variant_is_of_type (info, G_VARIANT_TYPE_VARIANT))
    self->info = g_variant_get_variant (info);
  else
    self->info = g_variant_ref_sink (info);

  g_variant_lookup (self->info, "label", "&s", &self->label);
  g_variant_lookup (self->info, "detail", "&s", &self->detail);

  if (JSONRPC_MESSAGE_PARSE (self->info, "kind", JSONRPC_MESSAGE_GET_INT64 (&kind)))
    self->kind = CLAMP (kind, 0, G_MAXUINT);

  return self;
}
