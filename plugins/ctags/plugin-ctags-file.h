/* plugin-ctags-file.h
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

#include <libdex.h>

G_BEGIN_DECLS

#define PLUGIN_TYPE_CTAGS_FILE (plugin_ctags_file_get_type())

G_DECLARE_FINAL_TYPE (PluginCtagsFile, plugin_ctags_file, PLUGIN, CTAGS_FILE, GObject)

DexFuture *plugin_ctags_file_new         (GFile            *file);
GFile     *plugin_ctags_file_dup_file    (PluginCtagsFile  *self);
gsize      plugin_ctags_file_get_size    (PluginCtagsFile  *self);
void       plugin_ctags_file_peek_name   (PluginCtagsFile  *self,
                                          gsize             position,
                                          const char      **name,
                                          gsize            *name_len);
char      *plugin_ctags_file_dup_name     (PluginCtagsFile  *self,
                                           gsize             position);
void       plugin_ctags_file_peek_path   (PluginCtagsFile  *self,
                                          gsize             position,
                                          const char      **path,
                                          gsize            *path_len);
char      *plugin_ctags_file_dup_path     (PluginCtagsFile  *self,
                                           gsize             position);
void       plugin_ctags_file_peek_pattern (PluginCtagsFile  *self,
                                           gsize             position,
                                           const char      **pattern,
                                           gsize            *pattern_len);
char      *plugin_ctags_file_dup_pattern  (PluginCtagsFile  *self,
                                           gsize             position);
void       plugin_ctags_file_peek_keyval  (PluginCtagsFile  *self,
                                           gsize             position,
                                           const char      **keyval,
                                           gsize            *keyval_len);
char      *plugin_ctags_file_dup_keyval   (PluginCtagsFile  *self,
                                           gsize             position);

G_END_DECLS
