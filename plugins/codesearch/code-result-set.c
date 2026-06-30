/* code-result-set.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "code-index-private.h"
#include "code-query-private.h"
#include "code-result.h"
#include "code-result-set.h"

#define BATCH_SIZE 100

#define EGG_ARRAY_TYPE_NAME CodeResultArray
#define EGG_ARRAY_NAME code_result_array
#define EGG_ARRAY_ELEMENT_TYPE CodeResult *
#define EGG_ARRAY_FREE_FUNC g_object_unref
#include "../../contrib/eggarrayimpl.c"

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodeResultArray, code_result_array_clear)

struct _CodeResultSet
{
  GObject           parent_instance;
  CodeResultArray   matched;
  CodeQuery        *query;
  CodeIndex       **indexes;
  DexChannel       *channel;
  DexFuture        *receiver;
  DexScheduler     *scheduler;
  guint             n_indexes;
  guint             in_populate : 1;
  guint             did_populate : 1;
};

static guint
code_result_set_get_n_items (GListModel *model)
{
  CodeResultSet *self = CODE_RESULT_SET (model);
  gsize n_items = code_result_array_get_size (&self->matched);

  return n_items > G_MAXUINT ? G_MAXUINT : n_items;
}

static GType
code_result_set_get_item_type (GListModel *model)
{
  return CODE_TYPE_RESULT;
}

static gpointer
code_result_set_get_item (GListModel *model,
                          guint       position)
{
  CodeResultSet *self = CODE_RESULT_SET (model);

  if (position >= code_result_array_get_size (&self->matched))
    return NULL;

  return g_object_ref (code_result_array_get (&self->matched, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = code_result_set_get_n_items;
  iface->get_item_type = code_result_set_get_item_type;
  iface->get_item = code_result_set_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (CodeResultSet, code_result_set, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_N_ITEMS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

CodeResultSet *
code_result_set_new (CodeQuery         *query,
                     CodeIndex * const *indexes,
                     guint              n_indexes)
{
  CodeResultSet *self;

  g_return_val_if_fail (CODE_IS_QUERY (query), NULL);
  g_return_val_if_fail (indexes != NULL || n_indexes == 0, NULL);

  self = g_object_new (CODE_TYPE_RESULT_SET, NULL);
  self->query = g_object_ref (query);
  self->indexes = g_new0 (CodeIndex *, n_indexes);
  self->n_indexes = n_indexes;
  self->channel = dex_channel_new (G_MAXINT);

  for (guint i = 0; i < n_indexes; i++)
    self->indexes[i] = code_index_ref (indexes[i]);

  return self;
}

static void
code_result_set_finalize (GObject *object)
{
  CodeResultSet *self = (CodeResultSet *)object;

  dex_clear (&self->receiver);
  dex_clear (&self->scheduler);

  for (guint i = 0; i < self->n_indexes; i++)
    g_clear_pointer (&self->indexes[i], code_index_unref);

  g_clear_pointer (&self->indexes, g_free);
  code_result_array_clear (&self->matched);
  g_clear_object (&self->query);

  G_OBJECT_CLASS (code_result_set_parent_class)->finalize (object);
}

static void
code_result_set_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CodeResultSet *self = CODE_RESULT_SET (object);

  switch (prop_id)
    {
    case PROP_N_ITEMS:
      g_value_set_uint (value, g_list_model_get_n_items (G_LIST_MODEL (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
code_result_set_class_init (CodeResultSetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = code_result_set_finalize;
  object_class->get_property = code_result_set_get_property;

  properties [PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
code_result_set_init (CodeResultSet *self)
{
  code_result_array_init (&self->matched);
}

static gboolean
code_result_set_await_task_group (DexTaskGroup **group,
                                  guint         *n_futures,
                                  GError       **error)
{
  g_autoptr(DexFuture) completed = NULL;

  g_assert (group != NULL);
  g_assert (n_futures != NULL);

  if (*group == NULL)
    return TRUE;

  completed = dex_task_group_close (*group);

  if (!dex_await (g_steal_pointer (&completed), error))
    {
      dex_clear (group);
      *n_futures = 0;
      return FALSE;
    }

  dex_clear (group);
  *n_futures = 0;

  return TRUE;
}

static gboolean
code_result_set_populate_from_index (CodeResultSet     *self,
                                     CodeIndex         *index,
                                     CodePostingQuery  *posting_query,
                                     GError           **error)
{
  g_autoptr(DexTaskGroup) group = NULL;
  g_auto(CodePostingList) document_ids;
  const guint *ids;
  guint n_futures = 0;
  gsize n_ids;

  g_assert (CODE_IS_RESULT_SET (self));
  g_assert (index != NULL);
  g_assert (posting_query != NULL);

  code_posting_list_init (&document_ids);

  if (!_code_posting_query_execute (posting_query, index, &document_ids))
    return FALSE;

  ids = code_posting_list_get_data (&document_ids);
  n_ids = code_posting_list_get_size (&document_ids);

  for (gsize i = 0; i < n_ids; i++)
    {
      const char *path;

      if (!(path = code_index_get_document_path (index, ids[i])))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "Posting query returned invalid document id %u",
                       ids[i]);

          if (group != NULL)
            dex_task_group_cancel (group);

          return FALSE;
        }

      if (group == NULL)
        group = dex_task_group_new (DEX_TASK_GROUP_FLAGS_NONE);

      if (!dex_task_group_add (group,
                               _code_query_match (self->query,
                                                  index,
                                                  path,
                                                  self->channel,
                                                  self->scheduler)))
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to add query match to task group");
          dex_clear (&group);
          return FALSE;
        }

      n_futures++;

      if (n_futures == BATCH_SIZE &&
          !code_result_set_await_task_group (&group, &n_futures, error))
        return FALSE;
    }

  return code_result_set_await_task_group (&group, &n_futures, error);
}

static DexFuture *
code_result_set_populate_fiber (gpointer user_data)
{
  CodeResultSet *self = user_data;
  g_autoptr(CodePostingQuery) posting_query = NULL;
  g_autoptr(GError) error = NULL;
  CodePostingQueryKind posting_query_kind;

  g_assert (CODE_IS_RESULT_SET (self));
  g_assert (CODE_IS_QUERY (self->query));
  g_assert (self->indexes != NULL);

  posting_query = _code_query_dup_posting_query (self->query);
  posting_query_kind = _code_posting_query_get_kind (posting_query);

  if (posting_query_kind == CODE_POSTING_QUERY_NONE)
    return dex_future_new_for_boolean (TRUE);

  for (guint i = 0; i < self->n_indexes; i++)
    {
      if (!code_result_set_populate_from_index (self,
                                                self->indexes[i],
                                                posting_query,
                                                &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
code_result_set_populate_completed (DexFuture *completed,
                                    gpointer   user_data)
{
  CodeResultSet *self = user_data;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (CODE_IS_RESULT_SET (self));

  self->in_populate = FALSE;
  self->did_populate = TRUE;

  dex_channel_close_send (self->channel);

  return NULL;
}

static DexFuture *
code_result_set_receive_fiber (gpointer user_data)
{
  CodeResultSet *self = user_data;

  g_assert (CODE_IS_RESULT_SET (self));

  for (;;)
    {
      g_autoptr(DexFuture) all = NULL;
      guint n_futures;
      gsize old_size;
      gsize new_size;

      /* If receive_all rejected, bail */
      all = dex_channel_receive_all (self->channel);
      if (!DEX_IS_FUTURE_SET (all))
        break;

      n_futures = dex_future_set_get_size (DEX_FUTURE_SET (all));
      old_size = code_result_array_get_size (&self->matched);

      for (guint i = 0; i < n_futures; i++)
        {
          const GValue *value = dex_future_set_get_value_at (DEX_FUTURE_SET (all), i, NULL);
          CodeResult *result = value ? g_value_get_object (value) : NULL;

          if (result != NULL)
            code_result_array_append (&self->matched, g_object_ref (result));
        }

      new_size = code_result_array_get_size (&self->matched);

      if (old_size < G_MAXUINT)
        {
          gsize added = MIN (new_size, G_MAXUINT) - old_size;

          if (added > 0)
            g_list_model_items_changed (G_LIST_MODEL (self), old_size, 0, added);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ITEMS]);

      /* Wait for another batch to come in */
      dex_await (dex_timeout_new_msec (50), NULL);
    }

  return NULL;
}

DexFuture *
code_result_set_populate (CodeResultSet *self,
                          DexScheduler  *scheduler)
{
  DexFuture *ret;

  g_return_val_if_fail (CODE_IS_RESULT_SET (self), NULL);

  if (self->in_populate || self->did_populate)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVAL,
                                  "Result set has already populated");

  if (self->n_indexes == 0)
    {
      self->did_populate = TRUE;
      return dex_future_new_for_boolean (TRUE);
    }

  self->in_populate = TRUE;

  self->scheduler = scheduler ? dex_ref (scheduler) : NULL;

  /* Start receiving results from channel on the fiber scheduler
   * for the current thread. This will add them to the result set
   * and emit ::items-changed(position,removed,added) as necessary.
   */
  self->receiver = dex_scheduler_spawn (NULL,
                                        0,
                                        code_result_set_receive_fiber,
                                        g_object_ref (self),
                                        g_object_unref);

  ret = dex_scheduler_spawn (scheduler,
                             0,
                             code_result_set_populate_fiber,
                             g_object_ref (self),
                             g_object_unref);
  ret = dex_future_finally (ret,
                            code_result_set_populate_completed,
                            g_object_ref (self),
                            g_object_unref);

  return ret;
}

/**
 * code_result_set_populate_async:
 * @self: an #CodeResultSet
 * @scheduler: (nullable): a #DexScheduler or %NULL
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Schedules @self to be populated with results from indexes.
 *
 * If @scheduler is set, then it will be used to execute the fibers
 * performing the query.
 */
void
code_result_set_populate_async (CodeResultSet       *self,
                                DexScheduler        *scheduler,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(DexAsyncResult) result = NULL;

  g_return_if_fail (CODE_IS_RESULT_SET (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!scheduler || DEX_IS_SCHEDULER (scheduler));

  result = dex_async_result_new (self, cancellable, callback, user_data);
  dex_async_result_await (result, code_result_set_populate (self, scheduler));
}

/**
 * code_result_set_populate_finish:
 * @self: an #CodeResultSet
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Returns:
 */
gboolean
code_result_set_populate_finish (CodeResultSet  *self,
                                 GAsyncResult   *result,
                                 GError        **error)
{
  g_return_val_if_fail (CODE_IS_RESULT_SET (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

void
code_result_set_cancel (CodeResultSet *self)
{
  g_return_if_fail (CODE_IS_RESULT_SET (self));

  /* Just close the send side of the channel so that anything
   * trying to queue a result into it will fail and cause all
   * of the work waiting to fail early.
   */
  dex_channel_close_send (self->channel);
}
