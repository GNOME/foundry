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
#include "foundry-text-buffer.h"
#include "foundry-text-document.h"
#include "foundry-text-iter.h"

enum {
  PROP_0,
  PROP_DOCUMENT,
  PROP_WORD,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryCompletionRequest, foundry_completion_request, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

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
      g_value_take_object (value, foundry_completion_request_dup_document (self));
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
 * foundry_completion_request_dup_document:
 * @self: a [class@Foundry.CompletionRequest]
 *
 * Returns: (transfer full): a [class@Foundry.TextDocument]
 */
FoundryTextDocument *
foundry_completion_request_dup_document (FoundryCompletionRequest *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (self), NULL);

  return FOUNDRY_COMPLETION_REQUEST_GET_CLASS (self)->dup_document (self);
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

/**
 * foundry_completion_request_dup_language_id:
 * @self: a [class@Foundry.CompletionRequest]
 *
 * Gets the language identifier for the completion request, such as "c" or "js".
 *
 * The language identifiers are expected to match GtkSourceView language identifiers.
 *
 * Returns: (transfer full) (nullable): the language identifier or %NULL
 */
char *
foundry_completion_request_dup_language_id (FoundryCompletionRequest *self)
{
  g_autoptr(FoundryTextDocument) document = NULL;
  g_autoptr(FoundryTextBuffer) buffer = NULL;

  g_return_val_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (self), NULL);

  if ((document = foundry_completion_request_dup_document (self)) &&
      (buffer = foundry_text_document_dup_buffer (document)))
    return foundry_text_buffer_dup_language_id (buffer);

  return NULL;
}

/**
 * foundry_completion_request_get_bounds:
 * @self: a [class@Foundry.CompletionRequest]
 * @begin: (out) (nullable): location for iter where completion started
 * @end: (out) (nullable): location for where completion request ended
 *
 * This gets the bounds for the completion request.
 *
 * Generally, `begin` will be right after a break character such as "." and
 * `end` will be where cursor is currently.
 */
void
foundry_completion_request_get_bounds (FoundryCompletionRequest *self,
                                       FoundryTextIter          *begin,
                                       FoundryTextIter          *end)
{
  FoundryTextIter dummy1;
  FoundryTextIter dummy2;

  g_return_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (self));

  if (begin == NULL)
    begin = &dummy1;

  if (end == NULL)
    end = &dummy2;

  FOUNDRY_COMPLETION_REQUEST_GET_CLASS (self)->get_bounds (self, begin, end);
}
