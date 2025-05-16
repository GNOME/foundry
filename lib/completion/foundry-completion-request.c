/* foundry-completion-request.c
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

#include "foundry-completion-request.h"

typedef struct
{
  gpointer dummy;
} FoundryCompletionRequestPrivate;

enum {
  PROP_0,
  PROP_DOCUMENT,
  PROP_WORD,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryCompletionRequest, foundry_completion_request, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_completion_request_finalize (GObject *object)
{
  G_OBJECT_CLASS (foundry_completion_request_parent_class)->finalize (object);
}

static void
foundry_completion_request_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  FoundryCompletionRequest *self = FOUNDRY_COMPLETION_REQUEST (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value, foundry_completion_request_get_document (self));
      break;

    case PROP_WORD:
      g_value_take_string (value, foundry_completion_request_dup_word (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_completion_request_class_init (FoundryCompletionRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_completion_request_finalize;
  object_class->get_property = foundry_completion_request_get_property;

  properties[PROP_DOCUMENT] =
    g_param_spec_object ("document", NULL, NULL,
                         FOUNDRY_TYPE_TEXT_DOCUMENT,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_WORD] =
    g_param_spec_string ("word", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_completion_request_init (FoundryCompletionRequest *self)
{
}

/**
 * foundry_completion_request_get_document:
 * @self: a [class@Foundry.CompletionRequest]
 *
 * Returns: (transfer none): a [class@Foundry.TextDocument]
 */
FoundryTextDocument *
foundry_completion_request_get_document (FoundryCompletionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (self), NULL);

  return FOUNDRY_COMPLETION_REQUEST_GET_CLASS (self)->get_document (self);
}

/**
 * foundry_completion_request_dup_word:
 * @self: a [class@Foundry.CompletionRequest]
 *
 * Returns: (transfer full) (nullable): the word to complete
 */
char *
foundry_completion_request_dup_word (FoundryCompletionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (self), NULL);

  return FOUNDRY_COMPLETION_REQUEST_GET_CLASS (self)->dup_word (self);
}
