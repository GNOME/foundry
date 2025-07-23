/* foundry-completion-proposal.c
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

#include "foundry-completion-proposal.h"

enum {
  PROP_0,
  PROP_DETAILS,
  PROP_ICON,
  PROP_TYPED_TEXT,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryCompletionProposal, foundry_completion_proposal, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_completion_proposal_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  FoundryCompletionProposal *self = FOUNDRY_COMPLETION_PROPOSAL (object);

  switch (prop_id)
    {
    case PROP_DETAILS:
      g_value_take_string (value, foundry_completion_proposal_dup_details (self));
      break;

    case PROP_ICON:
      g_value_take_object (value, foundry_completion_proposal_dup_icon (self));
      break;

    case PROP_TYPED_TEXT:
      g_value_take_string (value, foundry_completion_proposal_dup_typed_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_completion_proposal_class_init (FoundryCompletionProposalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_completion_proposal_get_property;

  properties[PROP_DETAILS] =
    g_param_spec_string ("details", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TYPED_TEXT] =
    g_param_spec_string ("typed-text", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_completion_proposal_init (FoundryCompletionProposal *self)
{
}

/**
 * foundry_completion_proposal_dup_typed_text:
 * @self: a [class@Foundry.CompletionProposal]
 *
 * Returns: (transfer full):
 */
char *
foundry_completion_proposal_dup_typed_text (FoundryCompletionProposal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMPLETION_PROPOSAL (self), NULL);

  return FOUNDRY_COMPLETION_PROPOSAL_GET_CLASS (self)->dup_typed_text (self);
}

char *
foundry_completion_proposal_dup_details (FoundryCompletionProposal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMPLETION_PROPOSAL (self), NULL);

  if (FOUNDRY_COMPLETION_PROPOSAL_GET_CLASS (self)->dup_details)
    return FOUNDRY_COMPLETION_PROPOSAL_GET_CLASS (self)->dup_details (self);

  return NULL;
}

/**
 * foundry_completion_proposal_dup_icon:
 * @self: a [class@Foundry.CompletionProposal]
 *
 * Returns: (transfer full) (nullable):
 */
GIcon *
foundry_completion_proposal_dup_icon (FoundryCompletionProposal *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMPLETION_PROPOSAL (self), NULL);

  if (FOUNDRY_COMPLETION_PROPOSAL_GET_CLASS (self)->dup_icon)
    return FOUNDRY_COMPLETION_PROPOSAL_GET_CLASS (self)->dup_icon (self);

  return NULL;
}
