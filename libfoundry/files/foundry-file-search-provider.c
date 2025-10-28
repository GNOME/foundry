/* foundry-file-search-provider.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-file-search-options.h"
#include "foundry-file-search-provider.h"
#include "foundry-operation.h"
#include "foundry-util.h"

/**
 * FoundryFileSearchProvider:
 *
 * A pluggable file search provider.
 *
 * The `FoundryFileSearchProvider` class allows precise implementation over how
 * file searches are performed. Plugins can provide an alternate implementation
 * to the default `grep` based search provider by implementing this class at
 * a higher priority than the `grep` provider.
 *
 * Since: 1.1
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
} FoundryFileSearchProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryFileSearchProvider, foundry_file_search_provider, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_file_search_provider_finalize (GObject *object)
{
  FoundryFileSearchProvider *self = (FoundryFileSearchProvider *)object;
  FoundryFileSearchProviderPrivate *priv = foundry_file_search_provider_get_instance_private (self);

  g_clear_object (&priv->plugin_info);

  G_OBJECT_CLASS (foundry_file_search_provider_parent_class)->finalize (object);
}

static void
foundry_file_search_provider_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  FoundryFileSearchProvider *self = FOUNDRY_FILE_SEARCH_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_take_object (value, foundry_file_search_provider_dup_plugin_info (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_search_provider_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  FoundryFileSearchProvider *self = FOUNDRY_FILE_SEARCH_PROVIDER (object);
  FoundryFileSearchProviderPrivate *priv = foundry_file_search_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_search_provider_class_init (FoundryFileSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_file_search_provider_finalize;
  object_class->get_property = foundry_file_search_provider_get_property;
  object_class->set_property = foundry_file_search_provider_set_property;

  /**
   * FoundryFileSearchProvider:plugin-info:
   *
   * Since: 1.1
   */
  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_file_search_provider_init (FoundryFileSearchProvider *self)
{
}

/**
 * foundry_file_search_provider_dup_plugin_info:
 * @self: a [class@Foundry.FileSearchProvider]
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
PeasPluginInfo *
foundry_file_search_provider_dup_plugin_info (FoundryFileSearchProvider *self)
{
  FoundryFileSearchProviderPrivate *priv = foundry_file_search_provider_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_FILE_SEARCH_PROVIDER (self), NULL);

  return priv->plugin_info ? g_object_ref (priv->plugin_info) : NULL;
}

/**
 * foundry_file_search_provider_search:
 * @self: a [class@Foundry.FileSearchProvider]
 * @options: a [class@Foundry.FileSearchOptions]
 * @operation: a [class@Foundry.Operation]
 *
 * Performs the requested search.
 *
 * It is expected that the implementation returns a [iface@Gio.ListModel] as
 * early as convenient and asynchronously populate the results after that.
 *
 * Callers should use [func@Foundry.list_model_await] to wait for completion
 * of the entire result set.
 *
 * If the provider is missing dependencies (such as a missing search tool in
 * the users path) this method should reject with `G_IO_ERROR_NOT_SUPPORTED`
 * so that the next implementation may be used.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.FileSearchMatch] or
 *    rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_file_search_provider_search (FoundryFileSearchProvider *self,
                                     FoundryFileSearchOptions  *options,
                                     FoundryOperation          *operation)
{
  dex_return_error_if_fail (FOUNDRY_IS_FILE_SEARCH_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_FILE_SEARCH_OPTIONS (options));
  dex_return_error_if_fail (FOUNDRY_IS_OPERATION (operation));

  if (FOUNDRY_FILE_SEARCH_PROVIDER_GET_CLASS (self)->search)
    return FOUNDRY_FILE_SEARCH_PROVIDER_GET_CLASS (self)->search (self, options, operation);

  return foundry_future_new_not_supported ();
}
