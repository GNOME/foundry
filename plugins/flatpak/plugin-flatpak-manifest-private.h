/* plugin-flatpak-manifest-private.h
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

#pragma once

#include "plugin-flatpak-manifest.h"

G_BEGIN_DECLS

struct _PluginFlatpakManifest
{
  FoundryConfig   parent_instance;
  FoundrySdk     *sdk_for_run;
  GFile          *file;
  char           *build_system;
  char           *command;
  char           *id;
  char           *primary_module_name;
  char           *runtime;
  char           *runtime_version;
  char           *sdk;
  char          **build_args;
  char          **primary_build_args;
  char          **primary_build_commands;
  char          **env;
  char          **primary_env;
  char          **x_run_args;
  char          **finish_args;
  char           *append_path;
  char           *prepend_path;
};

struct _PluginFlatpakManifestClass
{
  FoundryConfigClass parent_class;
};

void        _plugin_flatpak_manifest_set_id              (PluginFlatpakManifest   *self,
                                                          const char              *id);
void        _plugin_flatpak_manifest_set_runtime         (PluginFlatpakManifest   *self,
                                                          const char              *runtime);
void        _plugin_flatpak_manifest_set_runtime_version (PluginFlatpakManifest   *self,
                                                          const char              *runtime_version);
void        _plugin_flatpak_manifest_set_build_system    (PluginFlatpakManifest   *self,
                                                          const char              *build_system);
void        _plugin_flatpak_manifest_set_command         (PluginFlatpakManifest   *self,
                                                          const char              *command);
void        _plugin_flatpak_manifest_set_sdk             (PluginFlatpakManifest   *self,
                                                          const char              *sdk);
DexFuture  *_plugin_flatpak_manifest_resolve             (PluginFlatpakManifest   *self);

G_END_DECLS
