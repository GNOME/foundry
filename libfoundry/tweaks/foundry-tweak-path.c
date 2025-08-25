/* foundry-tweak-path.c
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

#include "foundry-tweak-path.h"

struct _FoundryTweakPath
{
  const char *path;
  char **parts;
  guint  n_parts;
};

FoundryTweakPath *
foundry_tweak_path_new (const char *path)
{
  FoundryTweakPath *self;
  g_autoptr(GStrvBuilder) builder = NULL;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (path[0] == '/', NULL);
  g_return_val_if_fail (g_str_has_suffix (path, "/"), NULL);

  builder = g_strv_builder_new ();

  for (const char *iter = path; *iter; iter = g_utf8_next_char (iter))
    {
      g_autofree char *part = NULL;
      const char *endptr;

      while (*iter == '/')
        iter++;

      if (*iter == 0)
        break;

      if (!(endptr = strchr (iter, '/')))
        g_assert_not_reached ();

      part = g_strndup (iter, endptr - iter);
      g_strv_builder_add (builder, part);
    }

  self = g_new0 (FoundryTweakPath, 1);
  self->path = g_intern_string (path);
  self->parts = g_strv_builder_end (builder);
  self->n_parts = g_strv_length (self->parts);

  return self;
}

void
foundry_tweak_path_free (FoundryTweakPath *self)
{
  g_free (self);
}

gboolean
foundry_tweak_path_has_prefix (const FoundryTweakPath *self,
                               const FoundryTweakPath *other)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  if (other->n_parts >= self->n_parts)
    return FALSE;

  for (guint i = 0; i < other->n_parts; i++)
    {
      if (!g_str_equal (self->parts[i], other->parts[i]))
        return FALSE;
    }

  return TRUE;
}

gboolean
foundry_tweak_path_equal (const FoundryTweakPath *self,
                          const FoundryTweakPath *other)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  if (other->n_parts != self->n_parts)
    return FALSE;

  for (guint i = 0; i < other->n_parts; i++)
    {
      if (!g_str_equal (self->parts[i], other->parts[i]))
        return FALSE;
    }

  return TRUE;
}

int
foundry_tweak_path_compute_depth (const FoundryTweakPath *self,
                                  const FoundryTweakPath *other)
{
  g_return_val_if_fail (self != NULL, -1);
  g_return_val_if_fail (other != NULL, -1);

  if (self->n_parts > other->n_parts)
    return -1;

  for (guint i = 0; i < self->n_parts; i++)
    {
      if (!g_str_equal (self->parts[i], other->parts[i]))
        return -1;
    }

  return other->n_parts - self->n_parts;
}

int
foundry_tweak_path_compare (const FoundryTweakPath *self,
                            const FoundryTweakPath *other)
{
  return strcmp (self->path, other->path);
}

FoundryTweakPath *
foundry_tweak_path_push (const FoundryTweakPath *self,
                         const char             *subpath)
{
  g_autofree char *path = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (subpath != NULL, NULL);

  while (*subpath == '/')
    subpath++;

  if (*subpath == 0)
    return NULL;

  path = g_strdup_printf ("%s%s%s", self->path, subpath,
                          g_str_has_suffix (subpath, "/") ? "" : "/");

  return foundry_tweak_path_new (path);
}

char *
foundry_tweak_path_dup_path (const FoundryTweakPath *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_strdup (self->path);
}
