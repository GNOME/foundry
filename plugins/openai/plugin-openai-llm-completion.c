/* plugin-openai-llm-completion.c
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

#include "foundry-json-input-stream-private.h"

#include "plugin-openai-llm-completion.h"
#include "plugin-openai-llm-completion-chunk.h"

struct _PluginOpenaiLlmCompletion
{
  FoundryLlmCompletion    parent_instance;
  FoundryJsonInputStream *stream;
  DexPromise             *finished;
};

G_DEFINE_FINAL_TYPE (PluginOpenaiLlmCompletion, plugin_openai_llm_completion, FOUNDRY_TYPE_LLM_COMPLETION)

static DexFuture *
plugin_openai_llm_completion_panic (PluginOpenaiLlmCompletion *self,
                                    GError                    *error)
{
  g_assert (PLUGIN_IS_OPENAI_LLM_COMPLETION (self));
  g_assert (error != NULL);

  if (dex_future_is_pending (DEX_FUTURE (self->finished)))
    dex_promise_reject (self->finished, g_error_copy (error));

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static void
read_line_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  DexPromise *promise = user_data;
  g_autoptr(GError) error = NULL;
  gsize length;
  g_autofree char *line = NULL;

  g_assert (G_IS_DATA_INPUT_STREAM (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  line = g_data_input_stream_read_line_finish (G_DATA_INPUT_STREAM (object), result, &length, &error);

  if (error != NULL)
    {
      dex_promise_reject (promise, g_steal_pointer (&error));
    }
  else if (line == NULL)
    {
      if (error == NULL)
        dex_promise_resolve_string (promise, NULL);
      else
        dex_promise_reject (promise, g_steal_pointer (&error));
    }
  else
    {
      if (length > 0 && line[length - 1] == '\n')
        {
          line[length - 1] = '\0';
          length--;
        }

      if (length > 0 && line[length - 1] == '\r')
        {
          line[length - 1] = '\0';
          length--;
        }

      dex_promise_resolve_string (promise, g_steal_pointer (&line));
    }
}

static DexFuture *
read_line_async (GDataInputStream *stream)
{
  DexPromise *promise;

  g_assert (G_IS_DATA_INPUT_STREAM (stream));

  promise = dex_promise_new_cancellable ();
  g_data_input_stream_read_line_async (stream,
                                       G_PRIORITY_DEFAULT,
                                       dex_promise_get_cancellable (promise),
                                       read_line_cb,
                                       dex_ref (promise));
  return DEX_FUTURE (promise);
}

static DexFuture *
plugin_openai_llm_completion_next_chunk_cb (DexFuture *completed,
                                            gpointer   user_data)
{
  PluginOpenaiLlmCompletion *self = user_data;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *line = NULL;
  const char *data_start;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (PLUGIN_IS_OPENAI_LLM_COMPLETION (self));
  g_assert (G_IS_INPUT_STREAM (self->stream));

  if (!(line = dex_await_string (dex_ref (completed), &error)))
    return plugin_openai_llm_completion_panic (self, g_steal_pointer (&error));

  /* OpenAI uses SSE format: "data: {...}\n\n" */
  /* Skip empty lines and non-data lines */
  if (g_str_has_prefix (line, "data: "))
    {
      data_start = line + strlen ("data: ");

      /* Handle "[DONE]" marker */
      if (g_str_equal (data_start, "[DONE]"))
        {
          if (dex_future_is_pending (DEX_FUTURE (self->finished)))
            dex_promise_resolve (self->finished, NULL);

          return dex_future_new_take_object (plugin_openai_llm_completion_chunk_new (NULL));
        }

      /* Parse JSON from the data line */
      {
        g_autoptr(JsonParser) parser = json_parser_new ();

        /* TODO: Would be better to do this on a fiber */
        if (!json_parser_load_from_data (parser, data_start, -1, &error))
          return plugin_openai_llm_completion_panic (self, g_steal_pointer (&error));

        if ((node = json_parser_get_root (parser)))
          json_node_ref (node);
      }

      if (node == NULL)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "Failed to extract JSON root node");

      return dex_future_new_take_object (plugin_openai_llm_completion_chunk_new (node));
    }
  else if (g_str_equal (line, ""))
    {
      /* Empty line, continue reading */
      return dex_future_finally (read_line_async (G_DATA_INPUT_STREAM (self->stream)),
                                 plugin_openai_llm_completion_next_chunk_cb,
                                 g_object_ref (self),
                                 g_object_unref);
    }

  /* Skip non-data lines and continue */
  return dex_future_finally (read_line_async (G_DATA_INPUT_STREAM (self->stream)),
                             plugin_openai_llm_completion_next_chunk_cb,
                             g_object_ref (self),
                             g_object_unref);
}

static DexFuture *
plugin_openai_llm_completion_next_chunk (FoundryLlmCompletion *completion)
{
  PluginOpenaiLlmCompletion *self = (PluginOpenaiLlmCompletion *)completion;

  g_assert (PLUGIN_IS_OPENAI_LLM_COMPLETION (self));
  g_assert (FOUNDRY_IS_JSON_INPUT_STREAM (self->stream));

  /* Read line as string for SSE format (not JSON) */
  return dex_future_finally (read_line_async (G_DATA_INPUT_STREAM (self->stream)),
                             plugin_openai_llm_completion_next_chunk_cb,
                             g_object_ref (self),
                             g_object_unref);
}

static DexFuture *
plugin_openai_llm_completion_when_finished (FoundryLlmCompletion *completion)
{
  return dex_ref (PLUGIN_OPENAI_LLM_COMPLETION (completion)->finished);
}

static void
plugin_openai_llm_completion_finalize (GObject *object)
{
  PluginOpenaiLlmCompletion *self = (PluginOpenaiLlmCompletion *)object;

  g_clear_object (&self->stream);

  if (dex_future_is_pending (DEX_FUTURE (self->finished)))
    dex_promise_reject (self->finished,
                        g_error_new (G_IO_ERROR,
                                     G_IO_ERROR_CANCELLED,
                                     "Object disposed"));

  dex_clear (&self->finished);

  G_OBJECT_CLASS (plugin_openai_llm_completion_parent_class)->finalize (object);
}

static void
plugin_openai_llm_completion_class_init (PluginOpenaiLlmCompletionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLlmCompletionClass *llm_completion_class = FOUNDRY_LLM_COMPLETION_CLASS (klass);

  object_class->finalize = plugin_openai_llm_completion_finalize;

  llm_completion_class->when_finished = plugin_openai_llm_completion_when_finished;
  llm_completion_class->next_chunk = plugin_openai_llm_completion_next_chunk;
}

static void
plugin_openai_llm_completion_init (PluginOpenaiLlmCompletion *self)
{
  self->finished = dex_promise_new ();
}

PluginOpenaiLlmCompletion *
plugin_openai_llm_completion_new (FoundryJsonInputStream *stream)
{
  PluginOpenaiLlmCompletion *self;

  self = g_object_new (PLUGIN_TYPE_OPENAI_LLM_COMPLETION, NULL);
  self->stream = g_object_ref (stream);

  return g_steal_pointer (&self);
}
