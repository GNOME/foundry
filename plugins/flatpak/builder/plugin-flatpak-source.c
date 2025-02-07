/* plugin-flatpak-source.c
 *
 * Copyright 2015 Red Hat, Inc
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

#include "plugin-flatpak-source-private.h"
#include "plugin-flatpak-utils.h"

# include "plugin-flatpak-source-archive.h"
# include "plugin-flatpak-source-git.h"
#if 0
# include "plugin-flatpak-source-patch.h"
# include "plugin-flatpak-source-bzr.h"
# include "plugin-flatpak-source-svn.h"
# include "plugin-flatpak-source-file.h"
# include "plugin-flatpak-source-dir.h"
# include "plugin-flatpak-source-script.h"
# include "plugin-flatpak-source-inline.h"
# include "plugin-flatpak-source-shell.h"
# include "plugin-flatpak-source-extra-data.h"
#endif

G_DEFINE_ABSTRACT_TYPE (PluginFlatpakSource, plugin_flatpak_source, PLUGIN_TYPE_FLATPAK_SERIALIZABLE)

enum {
  PROP_0,
  PROP_DEST,
  PROP_ONLY_ARCHES,
  PROP_SKIP_ARCHES,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
plugin_flatpak_source_finalize (GObject *object)
{
  PluginFlatpakSource *self = PLUGIN_FLATPAK_SOURCE (object);

  g_clear_pointer (&self->dest, g_free);
  g_clear_pointer (&self->only_arches, g_strfreev);
  g_clear_pointer (&self->skip_arches, g_strfreev);

  G_OBJECT_CLASS (plugin_flatpak_source_parent_class)->finalize (object);
}

static void
plugin_flatpak_source_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  PluginFlatpakSource *self = PLUGIN_FLATPAK_SOURCE (object);

  switch (prop_id)
    {
    case PROP_DEST:
      g_value_take_string (value, plugin_flatpak_source_dup_dest (self));
      break;

    case PROP_ONLY_ARCHES:
      g_value_take_boxed (value, plugin_flatpak_source_dup_only_arches (self));
      break;

    case PROP_SKIP_ARCHES:
      g_value_take_boxed (value, plugin_flatpak_source_dup_skip_arches (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_source_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  PluginFlatpakSource *self = PLUGIN_FLATPAK_SOURCE (object);

  switch (prop_id)
    {
    case PROP_DEST:
      plugin_flatpak_source_set_dest (self, g_value_get_string (value));
      break;

    case PROP_ONLY_ARCHES:
      plugin_flatpak_source_set_only_arches (self, g_value_get_boxed (value));
      break;

    case PROP_SKIP_ARCHES:
      plugin_flatpak_source_set_skip_arches (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_flatpak_source_class_init (PluginFlatpakSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = plugin_flatpak_source_finalize;
  object_class->get_property = plugin_flatpak_source_get_property;
  object_class->set_property = plugin_flatpak_source_set_property;

  properties[PROP_DEST] =
    g_param_spec_string ("dest", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ONLY_ARCHES] =
    g_param_spec_boxed ("only-arches", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_SKIP_ARCHES] =
    g_param_spec_boxed ("skip-arches", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_flatpak_source_init (PluginFlatpakSource *self)
{
}

char *
plugin_flatpak_source_dup_dest (PluginFlatpakSource *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_SOURCE (self), NULL);

  return g_strdup (self->dest);
}

void
plugin_flatpak_source_set_dest (PluginFlatpakSource *self,
                                const char          *dest)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_SOURCE (self));

  if (g_set_str (&self->dest, dest))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DEST]);
}

char **
plugin_flatpak_source_dup_only_arches (PluginFlatpakSource *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_SOURCE (self), NULL);

  return g_strdupv (self->only_arches);
}

void
plugin_flatpak_source_set_only_arches (PluginFlatpakSource *self,
                                       const char * const  *only_arches)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_SOURCE (self));

  if (foundry_set_strv (&self->only_arches, only_arches))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ONLY_ARCHES]);
}

char **
plugin_flatpak_source_dup_skip_arches (PluginFlatpakSource *self)
{
  g_return_val_if_fail (PLUGIN_IS_FLATPAK_SOURCE (self), NULL);

  return g_strdupv (self->skip_arches);
}

void
plugin_flatpak_source_set_skip_arches (PluginFlatpakSource *self,
                                       const char * const  *skip_arches)
{
  g_return_if_fail (PLUGIN_IS_FLATPAK_SOURCE (self));

  if (foundry_set_strv (&self->skip_arches, skip_arches))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SKIP_ARCHES]);
}
