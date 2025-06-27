/* foundry-source-hover-provider.c
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

#include "foundry-source-hover-provider.h"

static void
foundry_source_hover_provider_populate_async (GtkSourceHoverProvider *self,
                                              GtkSourceHoverContext  *context,
                                              GtkSourceHoverDisplay  *display,
                                              GCancellable           *cancellable,
                                              GAsyncReadyCallback     callback,
                                              gpointer                user_data)
{
  g_autoptr(DexAsyncResult) result = NULL;
  DexFuture *future;

  g_assert (FOUNDRY_IS_SOURCE_HOVER_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_HOVER_CONTEXT (context));
  g_assert (GTK_SOURCE_IS_HOVER_DISPLAY (display));

  future = FOUNDRY_SOURCE_HOVER_PROVIDER_GET_CLASS (self)->populate (FOUNDRY_SOURCE_HOVER_PROVIDER (self), context, display);

  g_assert (!future || DEX_IS_FUTURE (future));

  if (future == NULL)
    future = dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_NOT_SUPPORTED,
                                    "`%s` did not return a valid future",
                                    G_OBJECT_TYPE_NAME (self));

  result = dex_async_result_new (self, cancellable, callback, user_data);
  dex_async_result_await (result, g_steal_pointer (&future));
}

static gboolean
foundry_source_hover_provider_populate_finish (GtkSourceHoverProvider  *self,
                                               GAsyncResult            *result,
                                               GError                 **error)
{
  g_assert (GTK_SOURCE_IS_HOVER_PROVIDER (self));
  g_assert (DEX_IS_ASYNC_RESULT (result));

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

static void
hover_provider_iface_init (GtkSourceHoverProviderInterface *iface)
{
  iface->populate_async = foundry_source_hover_provider_populate_async;
  iface->populate_finish = foundry_source_hover_provider_populate_finish;
}

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundrySourceHoverProvider, foundry_source_hover_provider, FOUNDRY_TYPE_CONTEXTUAL,
                                  G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

static void
foundry_source_hover_provider_class_init (FoundrySourceHoverProviderClass *klass)
{
}

static void
foundry_source_hover_provider_init (FoundrySourceHoverProvider *self)
{
}
