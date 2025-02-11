/* foundry-plugin-lsp-server.c
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

#include "foundry-plugin-lsp-server-private.h"

struct _FoundryPluginLspServer
{
  FoundryLspServer  parent_instance;
  PeasPluginInfo   *plugin_info;
};

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryPluginLspServer, foundry_plugin_lsp_server, FOUNDRY_TYPE_LSP_SERVER)

static GParamSpec *properties[N_PROPS];

static char *
foundry_plugin_lsp_server_dup_name (FoundryLspServer *lsp_server)
{
  FoundryPluginLspServer *self = FOUNDRY_PLUGIN_LSP_SERVER (lsp_server);

  return g_strdup (peas_plugin_info_get_name (self->plugin_info));
}

static char **
foundry_plugin_lsp_server_dup_languages (FoundryLspServer *lsp_server)
{
  FoundryPluginLspServer *self = FOUNDRY_PLUGIN_LSP_SERVER (lsp_server);
  const char *x_languages = peas_plugin_info_get_external_data (self->plugin_info, "LSP-Languages");

  if (x_languages == NULL)
    return g_new0 (char *, 1);

  return g_strsplit (x_languages, ";", 0);
}

static void
foundry_plugin_lsp_server_finalize (GObject *object)
{
  FoundryPluginLspServer *self = (FoundryPluginLspServer *)object;

  g_clear_object (&self->plugin_info);

  G_OBJECT_CLASS (foundry_plugin_lsp_server_parent_class)->finalize (object);
}

static void
foundry_plugin_lsp_server_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  FoundryPluginLspServer *self = FOUNDRY_PLUGIN_LSP_SERVER (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_object (value, self->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_plugin_lsp_server_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  FoundryPluginLspServer *self = FOUNDRY_PLUGIN_LSP_SERVER (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      self->plugin_info = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_plugin_lsp_server_class_init (FoundryPluginLspServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryLspServerClass *lsp_server_class = FOUNDRY_LSP_SERVER_CLASS (klass);

  object_class->finalize = foundry_plugin_lsp_server_finalize;
  object_class->get_property = foundry_plugin_lsp_server_get_property;
  object_class->set_property = foundry_plugin_lsp_server_set_property;

  lsp_server_class->dup_name = foundry_plugin_lsp_server_dup_name;
  lsp_server_class->dup_languages = foundry_plugin_lsp_server_dup_languages;

  properties[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         PEAS_TYPE_PLUGIN_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_plugin_lsp_server_init (FoundryPluginLspServer *self)
{
}

FoundryLspServer *
foundry_plugin_lsp_server_new (FoundryContext *context,
                               PeasPluginInfo *plugin_info)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (PEAS_IS_PLUGIN_INFO (plugin_info), NULL);

  return g_object_new (FOUNDRY_TYPE_PLUGIN_LSP_SERVER,
                       "context", context,
                       "plugin-info", plugin_info,
                       NULL);
}
