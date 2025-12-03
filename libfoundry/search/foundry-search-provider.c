/* foundry-search-provider.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n-lib.h>
#include <libpeas.h>

#include "foundry-search-provider-private.h"
#include "foundry-search-request.h"

/**
 * FoundrySearchProvider:
 *
 * Abstract base class allowing plugins to provide search capabilities.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;
} FoundrySearchProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundrySearchProvider, foundry_search_provider, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_search_provider_real_load (FoundrySearchProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_search_provider_real_unload (FoundrySearchProvider *self)
{
  return dex_future_new_true ();
}

static void
foundry_search_provider_dispose (GObject *object)
{
  FoundrySearchProvider *self = (FoundrySearchProvider *)object;
  FoundrySearchProviderPrivate *priv = foundry_search_provider_get_instance_private (self);

  g_clear_object (&priv->plugin_info);

  G_OBJECT_CLASS (foundry_search_provider_parent_class)->dispose (object);
}

static void
foundry_search_provider_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  FoundrySearchProvider *self = FOUNDRY_SEARCH_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_take_object (value, foundry_search_provider_dup_plugin_info (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_search_provider_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  FoundrySearchProvider *self = FOUNDRY_SEARCH_PROVIDER (object);
  FoundrySearchProviderPrivate *priv = foundry_search_provider_get_instance_private (self);

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
foundry_search_provider_class_init (FoundrySearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_search_provider_dispose;
  object_class->get_property = foundry_search_provider_get_property;
  object_class->set_property = foundry_search_provider_set_property;

  klass->load = foundry_search_provider_real_load;
  klass->unload = foundry_search_provider_real_unload;

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_search_provider_init (FoundrySearchProvider *self)
{
}

/**
 * foundry_search_provider_load:
 * @self: a #FoundrySearchProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_search_provider_load (FoundrySearchProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SEARCH_PROVIDER (self), NULL);

  return FOUNDRY_SEARCH_PROVIDER_GET_CLASS (self)->load (self);
}

/**
 * foundry_search_provider_unload:
 * @self: a #FoundrySearchProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_search_provider_unload (FoundrySearchProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SEARCH_PROVIDER (self), NULL);

  return FOUNDRY_SEARCH_PROVIDER_GET_CLASS (self)->unload (self);
}

/**
 * foundry_search_provider_dup_name:
 * @self: a #FoundrySearchProvider
 *
 * Gets a name for the provider that is expected to be displayed to
 * users such as "Flatpak".
 *
 * Returns: (transfer full): the name of the provider
 */
char *
foundry_search_provider_dup_name (FoundrySearchProvider *self)
{
  char *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_SEARCH_PROVIDER (self), NULL);

  if (FOUNDRY_SEARCH_PROVIDER_GET_CLASS (self)->dup_name)
    ret = FOUNDRY_SEARCH_PROVIDER_GET_CLASS (self)->dup_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return g_steal_pointer (&ret);
}

/**
 * foundry_search_provider_dup_plugin_info:
 * @self: a [class@Foundry.SearchProvider]
 *
 * Returns: (transfer full) (nullable): a [class@Peas.PluginInfo]
 *
 * Since: 1.1
 */
PeasPluginInfo *
foundry_search_provider_dup_plugin_info (FoundrySearchProvider *self)
{
  FoundrySearchProviderPrivate *priv = foundry_search_provider_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SEARCH_PROVIDER (self), NULL);

  return priv->plugin_info ? g_object_ref (priv->plugin_info) : NULL;
}

/**
 * foundry_search_provider_search:
 * @self: a [class@Foundry.SearchProvider]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [iface@Gio.ListModel] of [class@Foundry.SearchResult] or
 *   rejects with error.
 */
DexFuture *
foundry_search_provider_search (FoundrySearchProvider *self,
                                FoundrySearchRequest  *request)
{
  dex_return_error_if_fail (FOUNDRY_IS_SEARCH_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_SEARCH_REQUEST (request));

  return FOUNDRY_SEARCH_PROVIDER_GET_CLASS (self)->search (self, request);
}
