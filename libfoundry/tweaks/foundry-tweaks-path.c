/* foundry-tweaks-path.c
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

#include "foundry-tweaks-path.h"

struct _FoundryTweaksPath
{
  char **elements;
  guint n_elements;
  FoundryTweaksPathMode mode : 2;
};

static inline FoundryTweaksPath *
foundry_tweaks_path_alloc (void)
{
  return g_atomic_rc_box_new0 (FoundryTweaksPath);
}

FoundryTweaksPath *
foundry_tweaks_path_ref (FoundryTweaksPath *self)
{
  return g_atomic_rc_box_acquire (self);
}

static void
foundry_tweaks_path_finalize (gpointer data)
{
  FoundryTweaksPath *self = data;
  g_clear_pointer (&self->elements, g_strfreev);
  self->n_elements = 0;
}

void
foundry_tweaks_path_unref (FoundryTweaksPath *self)
{
  return g_atomic_rc_box_release_full (self, foundry_tweaks_path_finalize);
}

FoundryTweaksPath *
foundry_tweaks_path_new (FoundryTweaksPathMode  mode,
                         const char * const    *path)
{
  FoundryTweaksPath *self;

  g_return_val_if_fail (mode <= FOUNDRY_TWEAKS_PATH_MODE_USER, NULL);

  self = foundry_tweaks_path_alloc ();
  self->elements = g_strdupv ((char **)path);
  self->n_elements = path ? g_strv_length ((char **)path) : 0;
  self->mode = mode;

  return self;
}

FoundryTweaksPath *
foundry_tweaks_path_push (const FoundryTweaksPath *self,
                          const char              *element)
{
  FoundryTweaksPath *copy;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (element != NULL, NULL);

  copy = foundry_tweaks_path_alloc ();
  copy->elements = g_new0 (char *, self->n_elements + 2);
  copy->n_elements = self->n_elements + 1;
  copy->mode = self->mode;

  for (guint i = 0; i < self->n_elements; i++)
    copy->elements[i] = g_strdup (self->elements[i]);
  copy->elements[self->n_elements] = g_strdup (element);

  return copy;
}

FoundryTweaksPath *
foundry_tweaks_path_pop (const FoundryTweaksPath *self)
{
  FoundryTweaksPath *copy;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->n_elements > 0, NULL);

  copy = foundry_tweaks_path_alloc ();
  copy->elements = g_new0 (char *, self->n_elements - 1);
  copy->n_elements = self->n_elements - 1;
  copy->mode = self->mode;

  for (guint i = 0; i < copy->n_elements; i++)
    copy->elements[i] = g_strdup (self->elements[i]);

  return copy;
}

gboolean
foundry_tweaks_path_equal (const FoundryTweaksPath *self,
                           const FoundryTweaksPath *prefix)
{
  if (self == prefix)
    return TRUE;

  if (self == NULL || prefix == NULL)
    return FALSE;

  if (prefix->n_elements != self->n_elements)
    return FALSE;

  if (prefix->mode != self->mode)
    return FALSE;

  for (guint i = 0; i < prefix->n_elements; i++)
    {
      if (strcmp (self->elements[i], prefix->elements[i]) != 0)
        return FALSE;
    }

  return TRUE;
}

gboolean
foundry_tweaks_path_has_prefix (const FoundryTweaksPath *self,
                                const FoundryTweaksPath *prefix)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (prefix != NULL, FALSE);

  if (prefix->n_elements >= self->n_elements)
    return FALSE;

  if (prefix->mode != self->mode)
    return FALSE;

  for (guint i = 0; i < prefix->n_elements; i++)
    {
      if (strcmp (prefix->elements[i], self->elements[i]) != 0)
        return FALSE;
    }

  return TRUE;
}

const char *
foundry_tweaks_path_get_element (const FoundryTweaksPath *self,
                                 guint                    position)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (position < self->n_elements, NULL);

  return self->elements[position];
}

guint
foundry_tweaks_path_get_length (const FoundryTweaksPath *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->n_elements;
}

gboolean
foundry_tweaks_path_is_root (const FoundryTweaksPath *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->n_elements == 0;
}

FoundryTweaksPathMode
foundry_tweaks_path_get_mode (const FoundryTweaksPath *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->mode;
}

G_DEFINE_BOXED_TYPE (FoundryTweaksPath, foundry_tweaks_path,
                     foundry_tweaks_path_ref, foundry_tweaks_path_unref)
G_DEFINE_ENUM_TYPE (FoundryTweaksPathMode, foundry_tweaks_path_mode,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_TWEAKS_PATH_MODE_DEFAULTS, "defaults"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_TWEAKS_PATH_MODE_PROJECT, "project"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_TWEAKS_PATH_MODE_USER, "user"))
