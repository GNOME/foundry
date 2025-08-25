/* foundry-tweak-info.h
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

G_BEGIN_DECLS

typedef enum _FoundryTweakAvailability
{
  FOUNDRY_TWEAK_AVAILABILITY_DEFAULTS = 1 << 0,
  FOUNDRY_TWEAK_AVAILABILITY_PROJECT  = 1 << 1,
  FOUNDRY_TWEAK_AVAILABILITY_USER     = 1 << 2,
  FOUNDRY_TWEAK_AVAILABILITY_ANY      = (FOUNDRY_TWEAK_AVAILABILITY_DEFAULTS | FOUNDRY_TWEAK_AVAILABILITY_PROJECT | FOUNDRY_TWEAK_AVAILABILITY_USER),
} FoundryTweakAvailability;

typedef enum _FoundryTweakType
{
  FOUNDRY_TWEAK_TYPE_GROUP  = 1,
  FOUNDRY_TWEAK_TYPE_SWITCH = 2,
} FoundryTweakType;

typedef enum _FoundryTweakSourceType
{
  FOUNDRY_TWEAK_SOURCE_TYPE_SETTING  = 1,
  FOUNDRY_TWEAK_SOURCE_TYPE_CALLBACK = 2,
} FoundryTweakSourceType;

typedef FoundryInput *(*FoundryTweakCallback) (const FoundryTweakInfo *info);

typedef struct _FoundryTweakSource
{
  FoundryTweakSourceType type;
  union {
    struct {
      const char *schema_id;
      const char *path;
      const char *key;
    } setting;
    struct {
      FoundryTweakCallback callback;
    } callback;
  };
} FoundryTweakSource;

struct _FoundryTweakInfo
{
  FoundryTweakType          type;
  FoundryTweakAvailability  availability;
  const char               *subpath;
  const char               *gettext_package;
  const char               *title;
  const char               *subtitle;
  const char               *icon_name;
  const char               *display_hint;
  const char               *sort_key;
  FoundryTweakSource       *source;

  /*< private >*/
  gpointer _reserved[4];
};

FoundryTweakInfo *foundry_tweak_info_copy (const FoundryTweakInfo *info);
void              foundry_tweak_info_free (FoundryTweakInfo       *info);

G_END_DECLS
