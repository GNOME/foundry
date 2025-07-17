/* foundry-terminal-palette-set.c
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

#include "foundry-terminal-palette-set.h"

struct _FoundryTerminalPaletteSet
{
  GObject parent_instance;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryTerminalPaletteSet, foundry_terminal_palette_set, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_terminal_palette_set_finalize (GObject *object)
{
  FoundryTerminalPaletteSet *self = (FoundryTerminalPaletteSet *)object;

  G_OBJECT_CLASS (foundry_terminal_palette_set_parent_class)->finalize (object);
}

static void
foundry_terminal_palette_set_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  FoundryTerminalPaletteSet *self = FOUNDRY_TERMINAL_PALETTE_SET (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_terminal_palette_set_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  FoundryTerminalPaletteSet *self = FOUNDRY_TERMINAL_PALETTE_SET (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_terminal_palette_set_class_init (FoundryTerminalPaletteSetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_terminal_palette_set_finalize;
  object_class->get_property = foundry_terminal_palette_set_get_property;
  object_class->set_property = foundry_terminal_palette_set_set_property;
}

static void
foundry_terminal_palette_set_init (FoundryTerminalPaletteSet *self)
{
}
