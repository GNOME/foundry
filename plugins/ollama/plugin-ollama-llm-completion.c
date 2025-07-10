/* plugin-ollama-llm-completion.c
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

#include <gio/gio.h>

#include "plugin-ollama-llm-completion.h"

struct _PluginOllamaLlmCompletion
{
  FoundryLlmCompletion    parent_instance;
  FoundryJsonInputStream *stream;
  DexPromise             *finished;
};

G_DEFINE_FINAL_TYPE (PluginOllamaLlmCompletion, plugin_ollama_llm_completion, FOUNDRY_TYPE_LLM_COMPLETION)

static DexFuture *
plugin_ollama_llm_completion_next_chunk (FoundryLlmCompletion *completion)
{
  PluginOllamaLlmCompletion *self = (PluginOllamaLlmCompletion *)completion;

  g_assert (PLUGIN_IS_OLLAMA_LLM_COMPLETION (self));

  return foundry_future_new_not_supported ();
}

static DexFuture *
plugin_ollama_llm_completion_when_finished (FoundryLlmCompletion *completion)
{
  return dex_ref (PLUGIN_OLLAMA_LLM_COMPLETION (completion)->finished);
}

static void
plugin_ollama_llm_completion_finalize (GObject *object)
{
  PluginOllamaLlmCompletion *self = (PluginOllamaLlmCompletion *)object;

  g_clear_object (&self->stream);

  if (dex_future_is_pending (DEX_FUTURE (self->finished)))
    dex_promise_reject (self->finished,
                        g_error_new (G_IO_ERROR,
                                     G_IO_ERROR_CANCELLED,
                                     "Object disposed"));

  dex_clear (&self->finished);

  G_OBJECT_CLASS (plugin_ollama_llm_completion_parent_class)->finalize (object);
}

static void
plugin_ollama_llm_completion_class_init (PluginOllamaLlmCompletionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmCompletionClass *llm_completion_class = FOUNDRY_LLM_COMPLETION_CLASS (klass);

  object_class->finalize = plugin_ollama_llm_completion_finalize;

  llm_completion_class->when_finished = plugin_ollama_llm_completion_when_finished;
  llm_completion_class->next_chunk = plugin_ollama_llm_completion_next_chunk;
}

static void
plugin_ollama_llm_completion_init (PluginOllamaLlmCompletion *self)
{
  self->finished = dex_promise_new ();
}

PluginOllamaLlmCompletion *
plugin_ollama_llm_completion_new (FoundryJsonInputStream *stream)
{
  PluginOllamaLlmCompletion *self;

  self = g_object_new (PLUGIN_TYPE_OLLAMA_LLM_COMPLETION, NULL);
  self->stream = g_object_ref (stream);

  return g_steal_pointer (&self);
}
