/* plugin-flatpak-builder-source.h
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

#pragma once

#include <foundry.h>
#include <json-glib/json-glib.h>

#include "plugin-flatpak-builder-cache.h"
#include "plugin-flatpak-builder-context.h"
#include "plugin-flatpak-builder-options.h"

G_BEGIN_DECLS

#define PLUGIN_TYPE_FLATPAK_BUILDER_SOURCE (plugin_flatpak_builder_source_get_type())

FOUNDRY_DECLARE_INTERNAL_TYPE (PluginFlatpakBuilderSource, plugin_flatpak_builder_source, PLUGIN, FLATPAK_BUILDER_SOURCE, GObject)

struct _PluginFlatpakBuilderSourceClass
{
  GObjectClass parent_class;

  DexFuture *(*download)  (PluginFlatpakBuilderSource  *self,
                           gboolean                     update_vcs,
                           PluginFlatpakBuilderContext *context);
  DexFuture *(*extract)   (PluginFlatpakBuilderSource  *self,
                           GFile                       *dest,
                           GFile                       *source_dir,
                           PluginFlatpakBuilderOptions *build_options,
                           PluginFlatpakBuilderContext *context);
  DexFuture *(*bundle)    (PluginFlatpakBuilderSource  *self,
                           PluginFlatpakBuilderContext *context);
  DexFuture *(*update)    (PluginFlatpakBuilderSource  *self,
                           PluginFlatpakBuilderContext *context);
  DexFuture *(*checksum)  (PluginFlatpakBuilderSource  *self,
                           PluginFlatpakBuilderCache   *cache,
                           PluginFlatpakBuilderContext *context);
  DexFuture *(*finish)    (PluginFlatpakBuilderSource  *self,
                           const char * const          *args,
                           PluginFlatpakBuilderContext *context);
  DexFuture *(*validate)  (PluginFlatpakBuilderSource  *self);
};

DexFuture *plugin_flatpak_builder_source_download        (PluginFlatpakBuilderSource  *self,
                                                          gboolean                     update_vcs,
                                                          PluginFlatpakBuilderContext *context) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_builder_source_extract         (PluginFlatpakBuilderSource  *self,
                                                          GFile                       *source_dir,
                                                          PluginFlatpakBuilderOptions *build_options,
                                                          PluginFlatpakBuilderContext *context) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_builder_source_bundle          (PluginFlatpakBuilderSource  *self,
                                                          PluginFlatpakBuilderContext *context) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_builder_source_update          (PluginFlatpakBuilderSource  *self,
                                                          PluginFlatpakBuilderContext *context) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_builder_source_checksum        (PluginFlatpakBuilderSource  *self,
                                                          PluginFlatpakBuilderCache   *cache,
                                                          PluginFlatpakBuilderContext *context) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_builder_source_finish          (PluginFlatpakBuilderSource  *self,
                                                          const char * const          *args,
                                                          PluginFlatpakBuilderContext *context) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_builder_source_validate        (PluginFlatpakBuilderSource  *self) G_GNUC_WARN_UNUSED_RESULT;
GFile     *plugin_flatpak_builder_source_dup_base_dir    (PluginFlatpakBuilderSource  *self) G_GNUC_WARN_UNUSED_RESULT;
void       plugin_flatpak_builder_source_set_base_dir    (PluginFlatpakBuilderSource  *self,
                                                          GFile                       *base_dir);
char      *plugin_flatpak_builder_source_dup_dest        (PluginFlatpakBuilderSource  *self) G_GNUC_WARN_UNUSED_RESULT;
void       plugin_flatpak_builder_source_set_dest        (PluginFlatpakBuilderSource  *self,
                                                          const char                  *dest);
char     **plugin_flatpak_builder_source_dup_only_arches (PluginFlatpakBuilderSource  *self) G_GNUC_WARN_UNUSED_RESULT;
void       plugin_flatpak_builder_source_set_only_arches (PluginFlatpakBuilderSource  *self,
                                                          const char * const          *only_arches);
char     **plugin_flatpak_builder_source_dup_skip_arches (PluginFlatpakBuilderSource  *self) G_GNUC_WARN_UNUSED_RESULT;
void       plugin_flatpak_builder_source_set_skip_arches (PluginFlatpakBuilderSource  *self,
                                                          const char * const          *skip_arches);
gboolean   plugin_flatpak_builder_source_is_enabled      (PluginFlatpakBuilderSource  *self,
                                                          PluginFlatpakBuilderContext *context);
DexFuture *plugin_flatpak_builder_source_from_json       (JsonNode                    *node) G_GNUC_WARN_UNUSED_RESULT;
JsonNode  *plugin_flatpak_builder_source_to_json         (PluginFlatpakBuilderSource  *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
