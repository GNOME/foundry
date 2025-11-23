/* foundry-vcs-diff-line.c
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

#include "foundry-vcs-diff-line.h"

G_DEFINE_ABSTRACT_TYPE (FoundryVcsDiffLine, foundry_vcs_diff_line, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ORIGIN,
  PROP_OLD_LINE,
  PROP_NEW_LINE,
  PROP_TEXT,
  PROP_HAS_NEWLINE,
  PROP_LENGTH,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_diff_line_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundryVcsDiffLine *self = FOUNDRY_VCS_DIFF_LINE (object);

  switch (prop_id)
    {
    case PROP_ORIGIN:
      g_value_set_enum (value, foundry_vcs_diff_line_get_origin (self));
      break;

    case PROP_OLD_LINE:
      g_value_set_uint (value, foundry_vcs_diff_line_get_old_line (self));
      break;

    case PROP_NEW_LINE:
      g_value_set_uint (value, foundry_vcs_diff_line_get_new_line (self));
      break;

    case PROP_TEXT:
      g_value_take_string (value, foundry_vcs_diff_line_dup_text (self));
      break;

    case PROP_HAS_NEWLINE:
      g_value_set_boolean (value, foundry_vcs_diff_line_get_has_newline (self));
      break;

    case PROP_LENGTH:
      g_value_set_ulong (value, foundry_vcs_diff_line_get_length (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_diff_line_class_init (FoundryVcsDiffLineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_vcs_diff_line_get_property;

  properties[PROP_ORIGIN] =
    g_param_spec_enum ("origin", NULL, NULL,
                       FOUNDRY_TYPE_VCS_DIFF_LINE_ORIGIN,
                       FOUNDRY_VCS_DIFF_LINE_ORIGIN_CONTEXT,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_OLD_LINE] =
    g_param_spec_uint ("old-line", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_NEW_LINE] =
    g_param_spec_uint ("new-line", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_HAS_NEWLINE] =
    g_param_spec_boolean ("has-newline", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_LENGTH] =
    g_param_spec_ulong ("length", NULL, NULL,
                        0, G_MAXULONG, 0,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_diff_line_init (FoundryVcsDiffLine *self)
{
}

FoundryVcsDiffLineOrigin
foundry_vcs_diff_line_get_origin (FoundryVcsDiffLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_LINE (self), 0);

  if (FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_origin)
    return FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_origin (self);

  return 0;
}

guint
foundry_vcs_diff_line_get_old_line (FoundryVcsDiffLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_LINE (self), 0);

  if (FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_old_line)
    return FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_old_line (self);

  return 0;
}

guint
foundry_vcs_diff_line_get_new_line (FoundryVcsDiffLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_LINE (self), 0);

  if (FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_new_line)
    return FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_new_line (self);

  return 0;
}

char *
foundry_vcs_diff_line_dup_text (FoundryVcsDiffLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_LINE (self), NULL);

  if (FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->dup_text)
    return FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->dup_text (self);

  return NULL;
}

gboolean
foundry_vcs_diff_line_get_has_newline (FoundryVcsDiffLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_LINE (self), FALSE);

  if (FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_has_newline)
    return FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_has_newline (self);

  return FALSE;
}

gsize
foundry_vcs_diff_line_get_length (FoundryVcsDiffLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_LINE (self), 0);

  if (FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_length)
    return FOUNDRY_VCS_DIFF_LINE_GET_CLASS (self)->get_length (self);

  return 0;
}

G_DEFINE_ENUM_TYPE (FoundryVcsDiffLineOrigin, foundry_vcs_diff_line_origin,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_DIFF_LINE_ORIGIN_ADDED, "added"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_DIFF_LINE_ORIGIN_DELETED, "deleted"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_DIFF_LINE_ORIGIN_CONTEXT, "context"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_DIFF_LINE_ORIGIN_CONTEXT_EOFNL, "context-eofnl"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_DIFF_LINE_ORIGIN_ADD_EOFNL, "add-eofnl"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_VCS_DIFF_LINE_ORIGIN_DEL_EOFNL, "del-eofnl"))
