/* plugin-flatpak-documentation-bundle.c
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

#include "plugin-flatpak-documentation-bundle.h"

struct _PluginFlatpakDocumentationBundle
{
  FoundryDocumentationBundle parent_instance;
  FlatpakInstallation *installation;
  FlatpakRef *ref;
  guint installed : 1;
};

enum {
  PROP_0,
  PROP_INSTALLATION,
  PROP_REF,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (PluginFlatpakDocumentationBundle, plugin_flatpak_documentation_bundle, FOUNDRY_TYPE_DOCUMENTATION_BUNDLE)

static GParamSpec *properties[N_PROPS];

static char *
plugin_flatpak_documentation_bundle_dup_title (FoundryDocumentationBundle *bundle)
{
  PluginFlatpakDocumentationBundle *self = PLUGIN_FLATPAK_DOCUMENTATION_BUNDLE (bundle);

  return g_strdup_printf ("%s %s",
                          flatpak_ref_get_name (self->ref),
                          flatpak_ref_get_branch (self->ref));
}

static gboolean
plugin_flatpak_documentation_bundle_get_installed (FoundryDocumentationBundle *bundle)
{
  PluginFlatpakDocumentationBundle *self = PLUGIN_FLATPAK_DOCUMENTATION_BUNDLE (bundle);

  return self->installed;
}

static DexFuture *
plugin_flatpak_documentation_bundle_install (FoundryDocumentationBundle *bundle,
                                             FoundryOperation           *operation)
{
  return dex_future_new_true ();
}

static void
plugin_flatpak_documentation_bundle_dispose (GObject *object)
{
  PluginFlatpakDocumentationBundle *self = (PluginFlatpakDocumentationBundle *)object;

  g_clear_object (&self->installation);
  g_clear_object (&self->ref);

  G_OBJECT_CLASS (plugin_flatpak_documentation_bundle_parent_class)->dispose (object);
}

static void
plugin_flatpak_documentation_bundle_get_property (GObject    *object,
                                                  guint       prop_id,
                                                  GValue     *value,
                                                  GParamSpec *pspec)
{
  PluginFlatpakDocumentationBundle *self = PLUGIN_FLATPAK_DOCUMENTATION_BUNDLE (object);

  switch (prop_id)
    {
    case PROP_INSTALLATION:
      g_value_set_object (value, self->installation);
      break;

    case PROP_REF:
      g_value_set_object (value, self->ref);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_documentation_bundle_set_property (GObject      *object,
                                                  guint         prop_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec)
{
  PluginFlatpakDocumentationBundle *self = PLUGIN_FLATPAK_DOCUMENTATION_BUNDLE (object);

  switch (prop_id)
    {
    case PROP_INSTALLATION:
      self->installation = g_value_dup_object (value);
      break;

    case PROP_REF:
      self->ref = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_documentation_bundle_class_init (PluginFlatpakDocumentationBundleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryDocumentationBundleClass *bundle_class = FOUNDRY_DOCUMENTATION_BUNDLE_CLASS (klass);

  object_class->dispose = plugin_flatpak_documentation_bundle_dispose;
  object_class->get_property = plugin_flatpak_documentation_bundle_get_property;
  object_class->set_property = plugin_flatpak_documentation_bundle_set_property;

  bundle_class->dup_title = plugin_flatpak_documentation_bundle_dup_title;
  bundle_class->get_installed = plugin_flatpak_documentation_bundle_get_installed;
  bundle_class->install = plugin_flatpak_documentation_bundle_install;

  properties[PROP_INSTALLATION] =
    g_param_spec_object ("installation", NULL, NULL,
                         FLATPAK_TYPE_INSTALLATION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_REF] =
    g_param_spec_object ("ref", NULL, NULL,
                         FLATPAK_TYPE_REF,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_documentation_bundle_init (PluginFlatpakDocumentationBundle *self)
{
}

FoundryDocumentationBundle *
plugin_flatpak_documentation_bundle_new (FoundryContext      *context,
                                         FlatpakInstallation *installation,
                                         FlatpakRef          *ref,
                                         gboolean             installed)
{
  PluginFlatpakDocumentationBundle *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FLATPAK_IS_INSTALLATION (installation), NULL);
  g_return_val_if_fail (FLATPAK_IS_REF (ref), NULL);

  self = g_object_new (PLUGIN_TYPE_FLATPAK_DOCUMENTATION_BUNDLE,
                       "context", context,
                       "installation", installation,
                       "ref", ref,
                       NULL);

  self->installed = !!installed;

  return FOUNDRY_DOCUMENTATION_BUNDLE (self);
}
