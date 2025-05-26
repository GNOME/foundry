/* foundry-lsp-completion-results.c
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

#include "foundry-lsp-client.h"
#include "foundry-lsp-completion-proposal-private.h"
#include "foundry-lsp-completion-results.h"

typedef struct _Item
{
  guint index;
  guint priority;
} Item;

#define EGG_ARRAY_NAME items
#define EGG_ARRAY_TYPE_NAME Items
#define EGG_ARRAY_ELEMENT_TYPE Item
#define EGG_ARRAY_BY_VALUE 1
#define EGG_ARRAY_NO_MEMSET
#include "eggarrayimpl.c"

struct _FoundryLspCompletionResults
{
  GObject           parent_instance;
  FoundryLspClient *client;
  GVariant         *reply;
  GVariant         *results;
  Items             items;
};

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static GType
foundry_lsp_completion_results_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_COMPLETION_PROPOSAL;
}

static guint
foundry_lsp_completion_results_get_n_items (GListModel *model)
{
  FoundryLspCompletionResults *self = FOUNDRY_LSP_COMPLETION_RESULTS (model);

  return items_get_size (&self->items);
}

static gpointer
foundry_lsp_completion_results_get_item (GListModel *model,
                                         guint       position)
{
  FoundryLspCompletionResults *self = FOUNDRY_LSP_COMPLETION_RESULTS (model);
  g_autoptr(GVariant) child = NULL;
  const Item *item;

  if (position >= items_get_size (&self->items))
    return NULL;

  item = items_index (&self->items, position);
  child = g_variant_get_child_value (self->results, item->index);

  if (child == NULL)
    return NULL;

  return _foundry_lsp_completion_proposal_new (child);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_lsp_completion_results_get_item_type;
  iface->get_n_items = foundry_lsp_completion_results_get_n_items;
  iface->get_item = foundry_lsp_completion_results_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryLspCompletionResults, foundry_lsp_completion_results, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_lsp_completion_results_dispose (GObject *object)
{
  FoundryLspCompletionResults *self = (FoundryLspCompletionResults *)object;

  g_clear_object (&self->client);
  g_clear_pointer (&self->reply, g_variant_unref);
  g_clear_pointer (&self->results, g_variant_unref);
  items_clear (&self->items);

  G_OBJECT_CLASS (foundry_lsp_completion_results_parent_class)->dispose (object);
}

static void
foundry_lsp_completion_results_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  FoundryLspCompletionResults *self = FOUNDRY_LSP_COMPLETION_RESULTS (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, self->client);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_lsp_completion_results_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  FoundryLspCompletionResults *self = FOUNDRY_LSP_COMPLETION_RESULTS (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      self->client = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_lsp_completion_results_class_init (FoundryLspCompletionResultsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_lsp_completion_results_dispose;
  object_class->get_property = foundry_lsp_completion_results_get_property;
  object_class->set_property = foundry_lsp_completion_results_set_property;

  properties[PROP_CLIENT] =
    g_param_spec_object ("client", NULL, NULL,
                         FOUNDRY_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_lsp_completion_results_init (FoundryLspCompletionResults *self)
{
  items_init (&self->items);
}

/**
 * foundry_lsp_completion_results_dup_client:
 * @self: a [class@Foundry.LspCompletionResults]
 *
 * Returns: (transfer full): a [class@Foundry.CompletionResults]
 */
FoundryLspClient *
foundry_lsp_completion_results_dup_client (FoundryLspCompletionResults *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LSP_COMPLETION_RESULTS (self), NULL);

  return g_object_ref (self->client);
}

static DexFuture *
foundry_lsp_completion_results_load (gpointer data)
{
  FoundryLspCompletionResults *self = data;
  gsize n_children;

  g_assert (FOUNDRY_IS_LSP_COMPLETION_RESULTS (self));

  n_children = g_variant_n_children (self->results);

  items_set_size (&self->items, n_children);

  for (gsize i = 0; i < n_children; i++)
    {
      Item *item = items_index (&self->items, i);

      item->index = i;
      item->priority = 0;
    }

  return dex_future_new_take_object (g_object_ref (self));
}

/**
 * foundry_lsp_completion_results_new:
 * @client: a [class@Foundry.LspClient]
 * @reply: the reply from the LSP-enabled server
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.LspCompletionResults].
 */
DexFuture *
foundry_lsp_completion_results_new (FoundryLspClient *client,
                                    GVariant         *reply)
{
  g_autoptr(FoundryLspCompletionResults) self = NULL;
  g_autoptr(GVariant) unboxed = NULL;
  g_autoptr(GVariant) items = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (client));
  dex_return_error_if_fail (reply != NULL);

  self = g_object_new (FOUNDRY_TYPE_LSP_COMPLETION_RESULTS,
                       "client", client,
                       NULL);

  self->reply = g_variant_ref_sink (reply);

  /* Possibly unwrap the {items: []} style result. */
  if (g_variant_is_of_type (reply, G_VARIANT_TYPE_VARDICT) &&
      (items = g_variant_lookup_value (reply, "items", NULL)))
    {
      if (g_variant_is_of_type (items, G_VARIANT_TYPE_VARIANT))
        self->results = g_variant_get_variant (items);
      else
        self->results = g_steal_pointer (&items);
    }

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                             foundry_lsp_completion_results_load,
                             g_steal_pointer (&self),
                             g_object_unref);
}
