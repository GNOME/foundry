/* foundry-completion-provider.c
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

#include "foundry-completion-provider-private.h"

typedef struct
{
  PeasPluginInfo *plugin_info;
} FoundryCompletionProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryCompletionProvider, foundry_completion_provider, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_completion_provider_real_complete (FoundryCompletionProvider *self,
                                           FoundryCompletionRequest  *request)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (request));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Not supported");
}

static DexFuture *
foundry_completion_provider_real_refilter (FoundryCompletionProvider *self,
                                           FoundryCompletionRequest  *request,
                                           GListModel                *model)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (request));
  dex_return_error_if_fail (G_IS_LIST_MODEL (model));

  return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->complete (self, request);
}

static void
foundry_completion_provider_dispose (GObject *object)
{
  FoundryCompletionProvider *self = (FoundryCompletionProvider *)object;
  FoundryCompletionProviderPrivate *priv = foundry_completion_provider_get_instance_private (self);

  g_clear_object (&priv->plugin_info);

  G_OBJECT_CLASS (foundry_completion_provider_parent_class)->dispose (object);
}

static void
foundry_completion_provider_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  FoundryCompletionProvider *self = FOUNDRY_COMPLETION_PROVIDER (object);
  FoundryCompletionProviderPrivate *priv = foundry_completion_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_object (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_completion_provider_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  FoundryCompletionProvider *self = FOUNDRY_COMPLETION_PROVIDER (object);
  FoundryCompletionProviderPrivate *priv = foundry_completion_provider_get_instance_private (self);

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
foundry_completion_provider_class_init (FoundryCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_completion_provider_dispose;
  object_class->get_property = foundry_completion_provider_get_property;
  object_class->set_property = foundry_completion_provider_set_property;

  klass->complete = foundry_completion_provider_real_complete;
  klass->refilter = foundry_completion_provider_real_refilter;

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-ifo", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_completion_provider_init (FoundryCompletionProvider *self)
{
}

/**
 * foundry_completion_provider_complete:
 * @self: a [class@Foundry.CompletionProvider]
 * @request: a [class@Foundry.CompletionRequest]
 *
 * Asynchronously generate a list of completion items.
 *
 * Returns: (transfer full): [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.CompletionProposal] or
 *   rejects with error.
 */
DexFuture *
foundry_completion_provider_complete (FoundryCompletionProvider *self,
                                      FoundryCompletionRequest  *request)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (request));

  return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->complete (self, request);
}

/**
 * foundry_completion_provider_refilter:
 * @self: a [class@Foundry.CompletionProvider]
 * @request: a [class@Foundry.CompletionRequest]
 * @model: a [iface@Gio.ListModel]
 *
 * Asynchronously refilter @model or provides an alternate result set.
 *
 * Returns: (transfer full): [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.CompletionProposal] or
 *   rejects with error.
 */
DexFuture *
foundry_completion_provider_refilter (FoundryCompletionProvider *self,
                                      FoundryCompletionRequest  *request,
                                      GListModel                *model)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_REQUEST (request));
  dex_return_error_if_fail (G_IS_LIST_MODEL (model));

  return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->refilter (self, request, model);
}

DexFuture *
_foundry_completion_provider_load (FoundryCompletionProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));

  if (FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->load)
    return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->load (self);

  return dex_future_new_true ();
}

DexFuture *
_foundry_completion_provider_unload (FoundryCompletionProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self));

  if (FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->unload)
    return FOUNDRY_COMPLETION_PROVIDER_GET_CLASS (self)->unload (self);

  return dex_future_new_true ();
}

/**
 * foundry_completion_provider_get_plugin_info:
 * @self: a [class@Foundry.CompletionProvider]
 *
 * Returns: (transfer none): a [class@Peas.PluginInfo]
 */
PeasPluginInfo *
foundry_completion_provider_get_plugin_info (FoundryCompletionProvider *self)
{
  FoundryCompletionProviderPrivate *priv = foundry_completion_provider_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMPLETION_PROVIDER (self), NULL);

  return priv->plugin_info;
}
