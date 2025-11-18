/* foundry-vcs-provider.c
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

#include "foundry-vcs-provider-private.h"
#include "foundry-vcs-private.h"

/**
 * FoundryVcsProvider:
 *
 * Abstract base class for plugins to implement a version control system
 * such as Git, Mercurial, Subversion, or CVS.
 */

typedef struct
{
  FoundryVcs *vcs;
  GListStore *store;
  PeasPluginInfo *plugin_info;
} FoundryVcsProviderPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (FoundryVcsProvider, foundry_vcs_provider, FOUNDRY_TYPE_CONTEXTUAL,
                                  G_ADD_PRIVATE (FoundryVcsProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_vcs_provider_real_load (FoundryVcsProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_vcs_provider_real_unload (FoundryVcsProvider *self)
{
  FoundryVcsProviderPrivate *priv = foundry_vcs_provider_get_instance_private (self);

  g_assert (FOUNDRY_IS_VCS_PROVIDER (self));

  g_list_store_remove_all (priv->store);

  return dex_future_new_true ();
}

static void
foundry_vcs_provider_finalize (GObject *object)
{
  FoundryVcsProvider *self = (FoundryVcsProvider *)object;
  FoundryVcsProviderPrivate *priv = foundry_vcs_provider_get_instance_private (self);

  g_clear_object (&priv->store);
  g_clear_object (&priv->plugin_info);

  G_OBJECT_CLASS (foundry_vcs_provider_parent_class)->finalize (object);
}

static void
foundry_vcs_provider_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryVcsProvider *self = FOUNDRY_VCS_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_take_object (value, foundry_vcs_provider_dup_plugin_info (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_provider_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  FoundryVcsProvider *self = FOUNDRY_VCS_PROVIDER (object);
  FoundryVcsProviderPrivate *priv = foundry_vcs_provider_get_instance_private (self);

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
foundry_vcs_provider_class_init (FoundryVcsProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_vcs_provider_finalize;
  object_class->get_property = foundry_vcs_provider_get_property;
  object_class->set_property = foundry_vcs_provider_set_property;

  klass->load = foundry_vcs_provider_real_load;
  klass->unload = foundry_vcs_provider_real_unload;

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_provider_init (FoundryVcsProvider *self)
{
  FoundryVcsProviderPrivate *priv = foundry_vcs_provider_get_instance_private (self);

  priv->store = g_list_store_new (FOUNDRY_TYPE_VCS);
}

/**
 * foundry_vcs_provider_load:
 * @self: a #FoundryVcsProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_vcs_provider_load (FoundryVcsProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_PROVIDER (self), NULL);

  return FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->load (self);
}

/**
 * foundry_vcs_provider_unload:
 * @self: a #FoundryVcsProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_vcs_provider_unload (FoundryVcsProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_PROVIDER (self), NULL);

  return FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->unload (self);
}

/**
 * foundry_vcs_provider_dup_name:
 * @self: a #FoundryVcsProvider
 *
 * Gets a name for the provider that is expected to be displayed to
 * users such as "Flatpak".
 *
 * Returns: (transfer full): the name of the provider
 */
char *
foundry_vcs_provider_dup_name (FoundryVcsProvider *self)
{
  char *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_VCS_PROVIDER (self), NULL);

  if (FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->dup_name)
    ret = FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->dup_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return g_steal_pointer (&ret);
}

/**
 * foundry_vcs_provider_supports_uri:
 * @self: a #FoundryVcsProvider
 *
 * Checks if a URI is supported by the VCS provider.
 *
 * This is useful to determine if you can get a downloader for a URI
 * to clone the repository.
 *
 * Returns: `true` if the URI is supported
 */
gboolean
foundry_vcs_provider_supports_uri (FoundryVcsProvider *self,
                                   const char         *uri_string)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_PROVIDER (self), FALSE);

  if (FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->supports_uri == NULL)
    return FALSE;

  return FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->supports_uri (self, uri_string);
}

/**
 * foundry_vcs_provider_dup_plugin_info:
 * @self: a [class@Foundry.VcsProvider]
 *
 * Gets the plugin the provider belongs to.
 *
 * Returns: (transfer full) (nullable): a [class@Peas.PluginInfo] or %NULL
 *
 * Since: 1.1
 */
PeasPluginInfo *
foundry_vcs_provider_dup_plugin_info (FoundryVcsProvider *self)
{
  FoundryVcsProviderPrivate *priv = foundry_vcs_provider_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_VCS_PROVIDER (self), NULL);

  return priv->plugin_info ? g_object_ref (priv->plugin_info) : NULL;
}

void
foundry_vcs_provider_set_vcs (FoundryVcsProvider *self,
                              FoundryVcs         *vcs)
{
  FoundryVcsProviderPrivate *priv = foundry_vcs_provider_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_VCS_PROVIDER (self));
  g_return_if_fail (!vcs || FOUNDRY_IS_VCS (vcs));

  if (priv->vcs == vcs)
    return;

  if (priv->vcs)
    {
      g_list_store_remove_all (priv->store);
      _foundry_vcs_set_provider (priv->vcs, NULL);
      g_clear_object (&priv->vcs);
    }

  if (vcs)
    {
      priv->vcs = g_object_ref (vcs);
      _foundry_vcs_set_provider (vcs, self);
      g_list_store_append (priv->store, vcs);
    }
}

static GType
foundry_vcs_provider_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_VCS;
}

static guint
foundry_vcs_provider_get_n_items (GListModel *model)
{
  FoundryVcsProvider *self = FOUNDRY_VCS_PROVIDER (model);
  FoundryVcsProviderPrivate *priv = foundry_vcs_provider_get_instance_private (self);

  return g_list_model_get_n_items (G_LIST_MODEL (priv->store));
}

static gpointer
foundry_vcs_provider_get_item (GListModel *model,
                               guint       position)
{
  FoundryVcsProvider *self = FOUNDRY_VCS_PROVIDER (model);
  FoundryVcsProviderPrivate *priv = foundry_vcs_provider_get_instance_private (self);

  return g_list_model_get_item (G_LIST_MODEL (priv->store), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_vcs_provider_get_item_type;
  iface->get_n_items = foundry_vcs_provider_get_n_items;
  iface->get_item = foundry_vcs_provider_get_item;
}
