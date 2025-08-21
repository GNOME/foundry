/* foundry-tweaks-path.h
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

#include <glib-object.h>

#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_TWEAKS_PATH               (foundry_tweaks_path_get_type())
#define FOUNDRY_TYPE_TWEAKS_PATH_MODE          (foundry_tweaks_path_mode_get_type())
#define FOUNDRY_TWEAKS_PATH_FOR_DEFAULTS(path) (foundry_tweaks_path_get_mode(path) == FOUNDRY_TWEAKS_PATH_MODE_DEFAULTS)
#define FOUNDRY_TWEAKS_PATH_FOR_USER(path)     (foundry_tweaks_path_get_mode(path) == FOUNDRY_TWEAKS_PATH_MODE_USER)
#define FOUNDRY_TWEAKS_PATH_FOR_PROJECT(path)  (foundry_tweaks_path_get_mode(path) == FOUNDRY_TWEAKS_PATH_MODE_PROJECT)

/**
 * FoundryTweaksPathMode:
 * @FOUNDRY_TWEAKS_PATH_MODE_DEFAULTS: defaults for the user which may be
 *   shared among multiple foundry-using applications. This is typically
 *   the user's default settings.
 * @FOUNDRY_TWEAKS_PATH_MODE_PROJECT: overrides for the specific project
 *   which should take precedence over the defaults.
 * @FOUNDRY_TWEAKS_PATH_MODE_USER: overrides to the project settings which
 *   are specific to the user.
 */
typedef enum _FoundryTweaksPathMode
{
  FOUNDRY_TWEAKS_PATH_MODE_DEFAULTS,
  FOUNDRY_TWEAKS_PATH_MODE_PROJECT,
  FOUNDRY_TWEAKS_PATH_MODE_USER,
} FoundryTweaksPathMode;

FOUNDRY_AVAILABLE_IN_ALL
GType                  foundry_tweaks_path_get_type      (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_ALL
GType                  foundry_tweaks_path_mode_get_type (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_ALL
FoundryTweaksPath     *foundry_tweaks_path_new           (FoundryTweaksPathMode    mode,
                                                          const char * const      *path) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
FoundryTweaksPath     *foundry_tweaks_path_new_root      (FoundryTweaksPathMode    mode);
FOUNDRY_AVAILABLE_IN_ALL
FoundryTweaksPathMode  foundry_tweaks_path_get_mode      (const FoundryTweaksPath *self);
FOUNDRY_AVAILABLE_IN_ALL
FoundryTweaksPath     *foundry_tweaks_path_ref           (FoundryTweaksPath       *self);
FOUNDRY_AVAILABLE_IN_ALL
void                   foundry_tweaks_path_unref         (FoundryTweaksPath       *self);
FOUNDRY_AVAILABLE_IN_ALL
FoundryTweaksPath     *foundry_tweaks_path_push          (const FoundryTweaksPath *self,
                                                          const char              *element) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
FoundryTweaksPath     *foundry_tweaks_path_pop           (const FoundryTweaksPath *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
gboolean               foundry_tweaks_path_equal         (const FoundryTweaksPath *self,
                                                          const FoundryTweaksPath *prefix);
FOUNDRY_AVAILABLE_IN_ALL
gboolean               foundry_tweaks_path_has_prefix    (const FoundryTweaksPath *self,
                                                          const FoundryTweaksPath *prefix);
FOUNDRY_AVAILABLE_IN_ALL
guint                  foundry_tweaks_path_get_length    (const FoundryTweaksPath *self);
FOUNDRY_AVAILABLE_IN_ALL
const char            *foundry_tweaks_path_get_element   (const FoundryTweaksPath *self,
                                                          guint                    position);
FOUNDRY_AVAILABLE_IN_ALL
gboolean               foundry_tweaks_path_is_root       (const FoundryTweaksPath *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FoundryTweaksPath, foundry_tweaks_path_unref)

G_END_DECLS
