/* foundry-llm-completion.c
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

#include "foundry-llm-completion.h"

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryLlmCompletion, foundry_llm_completion, G_TYPE_OBJECT)

//static GParamSpec *properties[N_PROPS];

static void
foundry_llm_completion_get_property (GObject    *object,
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
foundry_llm_completion_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_completion_class_init (FoundryLlmCompletionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_llm_completion_get_property;
  object_class->set_property = foundry_llm_completion_set_property;
}

static void
foundry_llm_completion_init (FoundryLlmCompletion *self)
{
}

/**
 * foundry_llm_completion_finished:
 * @self: a [class@Foundry.LlmCompletion]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value when the LLM completion has finished generating.
 */
DexFuture *
foundry_llm_completion_finished (FoundryLlmCompletion *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_LLM_COMPLETION (self));

  if (FOUNDRY_LLM_COMPLETION_GET_CLASS (self)->finished)
    return FOUNDRY_LLM_COMPLETION_GET_CLASS (self)->finished (self);

  return dex_future_new_true ();
}
