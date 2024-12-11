/* plugin-flatpak.h
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

#include <flatpak.h>
#include <foundry.h>
#include <libdex.h>

G_BEGIN_DECLS

DexFuture *plugin_flatpak_installation_new_system   (void)
  G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_installation_new_user     (void)
  G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_installation_new_private  (FoundryContext      *context)
  G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_installation_new_for_path (GFile               *path,
                                                     gboolean             user)
  G_GNUC_WARN_UNUSED_RESULT;
DexFuture *plugin_flatpak_installation_list_refs    (FlatpakInstallation *installation,
                                                     FlatpakQueryFlags    flags)
  G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
