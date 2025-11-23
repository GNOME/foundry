/* foundry-vcs-diff-hunk.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-vcs-diff-hunk.h"
#include "foundry-util.h"

G_DEFINE_ABSTRACT_TYPE (FoundryVcsDiffHunk, foundry_vcs_diff_hunk, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_HEADER,
  PROP_OLD_START,
  PROP_OLD_LINES,
  PROP_NEW_START,
  PROP_NEW_LINES,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_diff_hunk_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryVcsDiffHunk *self = FOUNDRY_VCS_DIFF_HUNK (object);

  switch (prop_id)
    {
    case PROP_HEADER:
      g_value_take_string (value, foundry_vcs_diff_hunk_dup_header (self));
      break;

    case PROP_OLD_START:
      g_value_set_uint (value, foundry_vcs_diff_hunk_get_old_start (self));
      break;

    case PROP_OLD_LINES:
      g_value_set_uint (value, foundry_vcs_diff_hunk_get_old_lines (self));
      break;

    case PROP_NEW_START:
      g_value_set_uint (value, foundry_vcs_diff_hunk_get_new_start (self));
      break;

    case PROP_NEW_LINES:
      g_value_set_uint (value, foundry_vcs_diff_hunk_get_new_lines (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_diff_hunk_class_init (FoundryVcsDiffHunkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_vcs_diff_hunk_get_property;

  properties[PROP_HEADER] =
    g_param_spec_string ("header", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_OLD_START] =
    g_param_spec_uint ("old-start", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_OLD_LINES] =
    g_param_spec_uint ("old-lines", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_NEW_START] =
    g_param_spec_uint ("new-start", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_NEW_LINES] =
    g_param_spec_uint ("new-lines", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_diff_hunk_init (FoundryVcsDiffHunk *self)
{
}

/**
 * foundry_vcs_diff_hunk_dup_header:
 * @self: a [class@Foundry.VcsDiffHunk]
 *
 * The header for the hunk which is the part after the second "@@".
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_vcs_diff_hunk_dup_header (FoundryVcsDiffHunk *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_HUNK (self), NULL);

  if (FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->dup_header)
    return FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->dup_header (self);

  return NULL;
}

/**
 * foundry_vcs_diff_hunk_get_old_start:
 * @self: a [class@Foundry.VcsDiffHunk]
 *
 * Gets the starting line number in the old file for this hunk.
 *
 * Returns: the starting line number in the old file
 *
 * Since: 1.1
 */
guint
foundry_vcs_diff_hunk_get_old_start (FoundryVcsDiffHunk *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_HUNK (self), 0);

  if (FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->get_old_start)
    return FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->get_old_start (self);

  return 0;
}

/**
 * foundry_vcs_diff_hunk_get_old_lines:
 * @self: a [class@Foundry.VcsDiffHunk]
 *
 * Gets the number of lines in the old file for this hunk.
 *
 * Returns: the number of lines in the old file
 *
 * Since: 1.1
 */
guint
foundry_vcs_diff_hunk_get_old_lines (FoundryVcsDiffHunk *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_HUNK (self), 0);

  if (FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->get_old_lines)
    return FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->get_old_lines (self);

  return 0;
}

/**
 * foundry_vcs_diff_hunk_get_new_start:
 * @self: a [class@Foundry.VcsDiffHunk]
 *
 * Gets the starting line number in the new file for this hunk.
 *
 * Returns: the starting line number in the new file
 *
 * Since: 1.1
 */
guint
foundry_vcs_diff_hunk_get_new_start (FoundryVcsDiffHunk *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_HUNK (self), 0);

  if (FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->get_new_start)
    return FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->get_new_start (self);

  return 0;
}

/**
 * foundry_vcs_diff_hunk_get_new_lines:
 * @self: a [class@Foundry.VcsDiffHunk]
 *
 * Gets the number of lines in the new file for this hunk.
 *
 * Returns: the number of lines in the new file
 *
 * Since: 1.1
 */
guint
foundry_vcs_diff_hunk_get_new_lines (FoundryVcsDiffHunk *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_HUNK (self), 0);

  if (FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->get_new_lines)
    return FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->get_new_lines (self);

  return 0;
}

/**
 * foundry_vcs_diff_hunk_list_lines:
 * @self: a [class@Foundry.VcsDiffHunk]
 *
 * List the lines within a hunk.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to an [iface@Gio.ListModel] of [class@Foundry.VcsDiffLine].
 */
DexFuture *
foundry_vcs_diff_hunk_list_lines (FoundryVcsDiffHunk *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_VCS_DIFF_HUNK (self));

  if (FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->list_lines)
    return FOUNDRY_VCS_DIFF_HUNK_GET_CLASS (self)->list_lines (self);

  return foundry_future_new_not_supported ();
}
