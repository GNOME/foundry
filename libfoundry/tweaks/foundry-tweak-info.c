/* foundry-tweak-info.c
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

#include "foundry-tweak-info.h"

FoundryTweakInfo *
foundry_tweak_info_copy (const FoundryTweakInfo *info)
{
  FoundryTweakInfo *copy;

  g_return_val_if_fail (info != NULL, NULL);

  copy = g_memdup2 (info, sizeof *info);
  copy->subpath = g_intern_string (info->subpath);
  copy->title = g_intern_string (info->title);
  copy->subtitle = g_intern_string (info->subtitle);
  copy->icon_name = g_intern_string (info->icon_name);
  copy->display_hint = g_intern_string (info->display_hint);

  if (info->source)
    {
      copy->source = g_memdup2 (info->source, sizeof *info->source);

      switch (copy->source->type)
        {
        case FOUNDRY_TWEAK_SOURCE_TYPE_SETTING:
          copy->source->setting.schema_id = g_intern_string (copy->source->setting.schema_id);
          copy->source->setting.path = g_intern_string (copy->source->setting.path);
          copy->source->setting.key = g_intern_string (copy->source->setting.key);
          break;

        case FOUNDRY_TWEAK_SOURCE_TYPE_CALLBACK:
          break;

        default:
          break;
        }
    }

  return copy;
}

void
foundry_tweak_info_free (FoundryTweakInfo *info)
{
  g_free (info->source);
  g_free (info);
}
