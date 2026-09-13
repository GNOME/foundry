/* foundry-vcs-content.c
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

#include "foundry-vcs-content-private.h"

struct _FoundryVcsContent
{
  GObject parent_instance;
  char *id;
  GBytes *bytes;
  guint present : 1;
};

G_DEFINE_FINAL_TYPE (FoundryVcsContent, foundry_vcs_content, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  PROP_PRESENT,
  PROP_SIZE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_vcs_content_finalize (GObject *object)
{
  FoundryVcsContent *self = (FoundryVcsContent *)object;

  g_clear_pointer (&self->bytes, g_bytes_unref);
  g_clear_pointer (&self->id, g_free);

  G_OBJECT_CLASS (foundry_vcs_content_parent_class)->finalize (object);
}

static void
foundry_vcs_content_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryVcsContent *self = FOUNDRY_VCS_CONTENT (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, foundry_vcs_content_dup_id (self));
      break;

    case PROP_PRESENT:
      g_value_set_boolean (value, foundry_vcs_content_get_present (self));
      break;

    case PROP_SIZE:
      g_value_set_uint64 (value, foundry_vcs_content_get_size (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_vcs_content_class_init (FoundryVcsContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_vcs_content_finalize;
  object_class->get_property = foundry_vcs_content_get_property;

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PRESENT] =
    g_param_spec_boolean ("present", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  properties[PROP_SIZE] =
    g_param_spec_uint64 ("size", NULL, NULL,
                         0, G_MAXUINT64, 0,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_vcs_content_init (FoundryVcsContent *self)
{
}

/**
 * foundry_vcs_content_get_present:
 * @self: a [class@Foundry.VcsContent]
 *
 * Checks if this side of a delta exists at the selected endpoint.
 *
 * Returns: %TRUE if content is present.
 *
 * Since: 1.3
 */
gboolean
foundry_vcs_content_get_present (FoundryVcsContent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_CONTENT (self), FALSE);

  return self->present;
}

/**
 * foundry_vcs_content_dup_id:
 * @self: a [class@Foundry.VcsContent]
 *
 * Gets a stable identifier for this content, such as a blob id or worktree
 * snapshot id.
 *
 * Returns: (transfer full) (nullable): the content identifier.
 *
 * Since: 1.3
 */
char *
foundry_vcs_content_dup_id (FoundryVcsContent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_CONTENT (self), NULL);

  return g_strdup (self->id);
}

/**
 * foundry_vcs_content_get_size:
 * @self: a [class@Foundry.VcsContent]
 *
 * Gets the byte length of the captured content.
 *
 * Returns: the content size, or zero when absent.
 *
 * Since: 1.3
 */
guint64
foundry_vcs_content_get_size (FoundryVcsContent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_CONTENT (self), 0);

  if (self->bytes == NULL)
    return 0;

  return g_bytes_get_size (self->bytes);
}

/**
 * foundry_vcs_content_dup_bytes:
 * @self: a [class@Foundry.VcsContent]
 *
 * Gets the captured bytes for this content.
 *
 * Returns: (transfer full) (nullable): the captured bytes, or %NULL if absent.
 *
 * Since: 1.3
 */
GBytes *
foundry_vcs_content_dup_bytes (FoundryVcsContent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_CONTENT (self), NULL);

  if (self->bytes == NULL)
    return NULL;

  return g_bytes_ref (self->bytes);
}

FoundryVcsContent *
_foundry_vcs_content_new_absent (const char *id)
{
  FoundryVcsContent *self;

  self = g_object_new (FOUNDRY_TYPE_VCS_CONTENT, NULL);
  self->id = g_strdup (id);

  return self;
}

FoundryVcsContent *
_foundry_vcs_content_new_present (const char *id,
                                  GBytes     *bytes)
{
  FoundryVcsContent *self;

  g_return_val_if_fail (bytes != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_VCS_CONTENT, NULL);
  self->id = g_strdup (id);
  self->bytes = g_bytes_ref (bytes);
  self->present = TRUE;

  return self;
}
