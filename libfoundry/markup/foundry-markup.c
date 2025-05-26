/* foundry-markup.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "foundry-markup.h"

struct _FoundryMarkup
{
  GBytes            *contents;
  FoundryMarkupKind  kind;
};

G_DEFINE_ENUM_TYPE (FoundryMarkupKind, foundry_markup_kind,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_MARKUP_KIND_PLAINTEXT, "plaintext"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_MARKUP_KIND_MARKDOWN, "markdown"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_MARKUP_KIND_HTML, "html"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_MARKUP_KIND_PANGO, "pango"))

G_DEFINE_BOXED_TYPE (FoundryMarkup,
                     foundry_markup,
                     foundry_markup_ref,
                     foundry_markup_unref)

FoundryMarkup *
foundry_markup_new (GBytes            *contents,
                    FoundryMarkupKind  kind)
{
  FoundryMarkup *self;

  g_return_val_if_fail (contents != NULL, NULL);
  g_return_val_if_fail (kind < FOUNDRY_MARKUP_KIND_LAST, NULL);

  self = g_atomic_rc_box_new0 (FoundryMarkup);
  self->contents = g_bytes_ref (contents);
  self->kind = kind;

  return self;
}

FoundryMarkup *
foundry_markup_ref (FoundryMarkup *self)
{
  return g_atomic_rc_box_acquire (self);
}

static void
foundry_markup_finalize (gpointer data)
{
  FoundryMarkup *self = data;

  g_clear_pointer (&self->contents, g_bytes_unref);
}

void
foundry_markup_unref (FoundryMarkup *self)
{
  g_atomic_rc_box_release_full (self, foundry_markup_finalize);
}

/**
 * foundry_markup_dup_contents:
 * @self: a #FoundryMarkup
 *
 * Gets the contents bytes.
 *
 * Returns: (transfer full): a #GBytes containing the markup
 */
GBytes *
foundry_markup_dup_contents (FoundryMarkup *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_bytes_ref (self->contents);
}

FoundryMarkupKind
foundry_markup_get_kind (FoundryMarkup *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->kind;
}
