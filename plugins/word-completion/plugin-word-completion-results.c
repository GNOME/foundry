/* plugin-word-completion-results.c
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

#include "line-reader-private.h"

#include "plugin-word-completion-proposal.h"
#include "plugin-word-completion-results.h"

#define WORD_MIN 3
#define _1_MSEC (G_USEC_PER_SEC/1000)

struct _PluginWordCompletionResults
{
  GObject    parent_instance;
  GBytes    *bytes;
  DexFuture *future;
  GSequence *sequence;
  char      *language_id;
  guint      cached_size;
};

static GType
plugin_word_completion_results_get_item_type (GListModel *model)
{
  return G_TYPE_OBJECT;
}

static guint
plugin_word_completion_results_get_n_items (GListModel *model)
{
  PluginWordCompletionResults *self = PLUGIN_WORD_COMPLETION_RESULTS (model);

  return self->cached_size;
}

static gpointer
plugin_word_completion_results_get_item (GListModel *model,
                                         guint       position)
{
  PluginWordCompletionResults *self = PLUGIN_WORD_COMPLETION_RESULTS (model);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->sequence, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;

  return plugin_word_completion_proposal_new (g_sequence_get (iter));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = plugin_word_completion_results_get_item_type;
  iface->get_n_items = plugin_word_completion_results_get_n_items;
  iface->get_item = plugin_word_completion_results_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (PluginWordCompletionResults, plugin_word_completion_results, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GRegex *regex;

static void
plugin_word_completion_results_add (PluginWordCompletionResults *self,
                                    const char                  *word)
{
  GSequenceIter *iter;

  g_assert (PLUGIN_IS_WORD_COMPLETION_RESULTS (self));
  g_assert (word != NULL);

  iter = g_sequence_search (self->sequence,
                            (gpointer)word,
                            (GCompareDataFunc)strcmp,
                            NULL);

  if (!g_sequence_iter_is_end (iter))
    {
      GSequenceIter *prev;

      if (strcmp (word, g_sequence_get (iter)) == 0)
        return;

      prev = g_sequence_iter_prev (iter);

      if (prev != iter && strcmp (word, g_sequence_get (prev)) == 0)
        return;
    }

  iter = g_sequence_insert_before (iter, g_ref_string_new (word));

  self->cached_size++;

  g_list_model_items_changed (G_LIST_MODEL (self),
                              g_sequence_iter_get_position (iter),
                              0, 1);
}

static void
plugin_word_completion_results_finalize (GObject *object)
{
  PluginWordCompletionResults *self = (PluginWordCompletionResults *)object;

  g_clear_pointer (&self->bytes, g_bytes_unref);
  g_clear_pointer (&self->sequence, g_sequence_free);
  g_clear_pointer (&self->language_id, g_free);

  dex_clear (&self->future);

  G_OBJECT_CLASS (plugin_word_completion_results_parent_class)->finalize (object);
}

static void
plugin_word_completion_results_class_init (PluginWordCompletionResultsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_word_completion_results_finalize;

  regex = g_regex_new ("\\w+", G_REGEX_OPTIMIZE, G_REGEX_MATCH_DEFAULT, NULL);
}

static void
plugin_word_completion_results_init (PluginWordCompletionResults *self)
{
  self->sequence = g_sequence_new ((GDestroyNotify)g_ref_string_release);
}

PluginWordCompletionResults *
plugin_word_completion_results_new (GBytes     *bytes,
                                    const char *language_id)
{
  PluginWordCompletionResults *self;

  g_return_val_if_fail (bytes != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_WORD_COMPLETION_RESULTS, NULL);
  self->bytes = g_bytes_ref (bytes);
  self->language_id = g_strdup (language_id);

  return self;
}

static DexFuture *
plugin_word_completion_results_fiber (gpointer data)
{
  PluginWordCompletionResults *self = data;
  gint64 next_deadline;
  LineReader reader;
  const char *line;
  gsize line_len;

  g_assert (PLUGIN_IS_WORD_COMPLETION_RESULTS (self));
  g_assert (self->bytes != NULL);

  next_deadline = g_get_monotonic_time () + _1_MSEC;

  line_reader_init_from_bytes (&reader, self->bytes);

  while ((line = line_reader_next (&reader, &line_len)))
    {
      g_autoptr(GMatchInfo) match_info = NULL;
      gint64 now;

      if (line_len < WORD_MIN)
        continue;

      /* TODO: This would be a great place to try to resolve `#include` in C files
       *       similar to what Vim does.
       */

      if (g_regex_match_full (regex, line, line_len, 0, G_REGEX_MATCH_DEFAULT, &match_info, NULL))
        {
          if (g_match_info_matches (match_info))
            {
              do
                {
                  g_autofree char *word = g_match_info_fetch (match_info, 0);

                  plugin_word_completion_results_add (self, word);
                }
              while (g_match_info_next (match_info, NULL));
            }
        }

      now = g_get_monotonic_time ();

      if (now > next_deadline)
        {
          dex_await (dex_timeout_new_deadline (now + _1_MSEC), NULL);
          next_deadline = g_get_monotonic_time () + _1_MSEC;
        }
    }

  return dex_future_new_true ();
}

DexFuture *
plugin_word_completion_results_await (PluginWordCompletionResults *self)
{
  g_return_val_if_fail (PLUGIN_IS_WORD_COMPLETION_RESULTS (self), NULL);

  if (self->future == NULL)
    self->future = dex_scheduler_spawn (NULL, 0,
                                        plugin_word_completion_results_fiber,
                                        g_object_ref (self),
                                        g_object_unref);

  return dex_ref (self->future);
}
