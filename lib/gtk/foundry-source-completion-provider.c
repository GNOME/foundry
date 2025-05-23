/* foundry-source-completion-provider.c
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

#include <gtksourceview/gtksource.h>

#include "foundry-source-completion-proposal-private.h"
#include "foundry-source-completion-provider.h"
#include "foundry-source-completion-request-private.h"

struct _FoundrySourceCompletionProvider
{
  GObject                    parent_instance;
  FoundryCompletionProvider *provider;
};

enum {
  PROP_0,
  PROP_PROVIDER,
  N_PROPS
};

static gpointer
map_completion_result (gpointer item,
                       gpointer user_data)
{
  g_autoptr(FoundryCompletionProposal) proposal = item;

  return foundry_source_completion_proposal_new (proposal);
}

static DexFuture *
map_completion_results (DexFuture *completed,
                        gpointer   user_data)
{
  g_autoptr(GListModel) model = dex_await_object (dex_ref (completed), NULL);

  return dex_future_new_take_object (gtk_map_list_model_new (g_steal_pointer (&model),
                                                             map_completion_result,
                                                             NULL, NULL));
}

static void
foundry_source_completion_provider_populate_async (GtkSourceCompletionProvider *provider,
                                                   GtkSourceCompletionContext  *context,
                                                   GCancellable                *cancellable,
                                                   GAsyncReadyCallback          callback,
                                                   gpointer                     user_data)
{
  FoundrySourceCompletionProvider *self = (FoundrySourceCompletionProvider *)provider;
  g_autoptr(FoundryCompletionRequest) request = NULL;
  g_autoptr(DexAsyncResult) result = NULL;
  g_autoptr(DexFuture) future = NULL;

  g_assert (FOUNDRY_IS_SOURCE_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (FOUNDRY_IS_COMPLETION_PROVIDER (self->provider));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  request = foundry_source_completion_request_new (context);
  future = foundry_completion_provider_complete (self->provider, request);

  result = dex_async_result_new (provider, cancellable, callback, user_data);
  dex_async_result_await (result,
                          dex_future_then (g_steal_pointer (&future),
                                           map_completion_results,
                                           NULL, NULL));
}

static GListModel *
foundry_source_completion_provider_populate_finish (GtkSourceCompletionProvider  *provider,
                                                    GAsyncResult                 *result,
                                                    GError                      **error)
{
  FoundrySourceCompletionProvider *self = (FoundrySourceCompletionProvider *)provider;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) local_error = NULL;

  g_assert (FOUNDRY_IS_SOURCE_COMPLETION_PROVIDER (self));
  g_assert (DEX_IS_ASYNC_RESULT (result));

  if ((model = dex_async_result_propagate_pointer (DEX_ASYNC_RESULT (result), &local_error)))
    g_debug ("%s populated with %u proposals",
             G_OBJECT_TYPE_NAME (self->provider),
             g_list_model_get_n_items (model));
  else
    g_debug ("%s failed to populate with error \"%s\"",
             G_OBJECT_TYPE_NAME (self->provider),
             local_error->message);

  if (local_error != NULL)
    g_propagate_error (error, g_steal_pointer (&local_error));

  return g_steal_pointer (&model);
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
  iface->populate_async = foundry_source_completion_provider_populate_async;
  iface->populate_finish = foundry_source_completion_provider_populate_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundrySourceCompletionProvider, foundry_source_completion_provider, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_source_completion_provider_dispose (GObject *object)
{
  FoundrySourceCompletionProvider *self = (FoundrySourceCompletionProvider *)object;

  g_clear_object (&self->provider);

  G_OBJECT_CLASS (foundry_source_completion_provider_parent_class)->dispose (object);
}

static void
foundry_source_completion_provider_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  FoundrySourceCompletionProvider *self = FOUNDRY_SOURCE_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      g_value_set_object (value, self->provider);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_completion_provider_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  FoundrySourceCompletionProvider *self = FOUNDRY_SOURCE_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      self->provider = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_completion_provider_class_init (FoundrySourceCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_source_completion_provider_dispose;
  object_class->get_property = foundry_source_completion_provider_get_property;
  object_class->set_property = foundry_source_completion_provider_set_property;

  properties[PROP_PROVIDER] =
    g_param_spec_object ("provider", NULL, NULL,
                         FOUNDRY_TYPE_COMPLETION_PROVIDER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_source_completion_provider_init (FoundrySourceCompletionProvider *self)
{
}

/**
 * foundry_source_completion_provider_new:
 *
 * Returns: (transfer full):
 */
GtkSourceCompletionProvider *
foundry_source_completion_provider_new (FoundryCompletionProvider *provider)
{
  g_return_val_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (provider), NULL);

  return g_object_new (FOUNDRY_TYPE_SOURCE_COMPLETION_PROVIDER,
                       "provider", provider,
                       NULL);
}
