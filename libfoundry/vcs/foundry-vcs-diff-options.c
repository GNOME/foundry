/* foundry-vcs-diff-options.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "foundry-vcs-diff-options.h"

struct _FoundryVcsDiffOptions
{
  GObject parent_instance;
  guint context_lines;
  guint include_untracked : 1;
};

G_DEFINE_FINAL_TYPE (FoundryVcsDiffOptions, foundry_vcs_diff_options, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTEXT_LINES,
  PROP_INCLUDE_UNTRACKED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_diff_options_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundryVcsDiffOptions *self = FOUNDRY_VCS_DIFF_OPTIONS (object);

  switch (prop_id)
    {
    case PROP_CONTEXT_LINES:
      g_value_set_uint (value, foundry_vcs_diff_options_get_context_lines (self));
      break;

    case PROP_INCLUDE_UNTRACKED:
      g_value_set_boolean (value, foundry_vcs_diff_options_get_include_untracked (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_diff_options_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  FoundryVcsDiffOptions *self = FOUNDRY_VCS_DIFF_OPTIONS (object);

  switch (prop_id)
    {
    case PROP_CONTEXT_LINES:
      foundry_vcs_diff_options_set_context_lines (self, g_value_get_uint (value));
      break;

    case PROP_INCLUDE_UNTRACKED:
      foundry_vcs_diff_options_set_include_untracked (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_diff_options_class_init (FoundryVcsDiffOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_vcs_diff_options_get_property;
  object_class->set_property = foundry_vcs_diff_options_set_property;

  properties[PROP_CONTEXT_LINES] =
    g_param_spec_uint ("context-lines", NULL, NULL,
                       0, G_MAXUINT, 3,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_INCLUDE_UNTRACKED] =
    g_param_spec_boolean ("include-untracked", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_diff_options_init (FoundryVcsDiffOptions *self)
{
  self->context_lines = 3;
}

/**
 * foundry_vcs_diff_options_new:
 *
 * Creates options for a VCS diff request.
 *
 * Returns: (transfer full): a new options object.
 *
 * Since: 1.3
 */
FoundryVcsDiffOptions *
foundry_vcs_diff_options_new (void)
{
  return g_object_new (FOUNDRY_TYPE_VCS_DIFF_OPTIONS, NULL);
}

/**
 * foundry_vcs_diff_options_copy:
 * @self: a [class@Foundry.VcsDiffOptions]
 *
 * Copies a diff options object.
 *
 * Returns: (transfer full): a copy of @self.
 *
 * Since: 1.3
 */
FoundryVcsDiffOptions *
foundry_vcs_diff_options_copy (FoundryVcsDiffOptions *self)
{
  FoundryVcsDiffOptions *copy;

  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_OPTIONS (self), NULL);

  copy = foundry_vcs_diff_options_new ();
  copy->context_lines = self->context_lines;
  copy->include_untracked = self->include_untracked;

  return copy;
}

guint
foundry_vcs_diff_options_get_context_lines (FoundryVcsDiffOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_OPTIONS (self), 3);

  return self->context_lines;
}

void
foundry_vcs_diff_options_set_context_lines (FoundryVcsDiffOptions *self,
                                            guint                  context_lines)
{
  g_return_if_fail (FOUNDRY_IS_VCS_DIFF_OPTIONS (self));

  if (self->context_lines != context_lines)
    {
      self->context_lines = context_lines;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTEXT_LINES]);
    }
}

gboolean
foundry_vcs_diff_options_get_include_untracked (FoundryVcsDiffOptions *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_DIFF_OPTIONS (self), FALSE);

  return self->include_untracked;
}

void
foundry_vcs_diff_options_set_include_untracked (FoundryVcsDiffOptions *self,
                                                gboolean               include_untracked)
{
  g_return_if_fail (FOUNDRY_IS_VCS_DIFF_OPTIONS (self));

  include_untracked = !!include_untracked;

  if (self->include_untracked != include_untracked)
    {
      self->include_untracked = include_untracked;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INCLUDE_UNTRACKED]);
    }
}
