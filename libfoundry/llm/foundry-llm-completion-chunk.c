/* foundry-llm-completion-chunk.c
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

#include "foundry-llm-completion-chunk.h"

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryLlmCompletionChunk, foundry_llm_completion_chunk, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_llm_completion_chunk_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  FoundryLlmCompletionChunk *self = FOUNDRY_LLM_COMPLETION_CHUNK (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_completion_chunk_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  FoundryLlmCompletionChunk *self = FOUNDRY_LLM_COMPLETION_CHUNK (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_completion_chunk_class_init (FoundryLlmCompletionChunkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_llm_completion_chunk_get_property;
  object_class->set_property = foundry_llm_completion_chunk_set_property;
}

static void
foundry_llm_completion_chunk_init (FoundryLlmCompletionChunk *self)
{
}
