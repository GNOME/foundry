/* foundry-terminal-palette.c
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

#include "foundry-terminal-palette-private.h"

struct _FoundryTerminalPalette
{
  GObject parent_instance;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryTerminalPalette, foundry_terminal_palette, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_terminal_palette_finalize (GObject *object)
{
  FoundryTerminalPalette *self = (FoundryTerminalPalette *)object;

  G_OBJECT_CLASS (foundry_terminal_palette_parent_class)->finalize (object);
}

static void
foundry_terminal_palette_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundryTerminalPalette *self = FOUNDRY_TERMINAL_PALETTE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_terminal_palette_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  FoundryTerminalPalette *self = FOUNDRY_TERMINAL_PALETTE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_terminal_palette_class_init (FoundryTerminalPaletteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_terminal_palette_finalize;
  object_class->get_property = foundry_terminal_palette_get_property;
  object_class->set_property = foundry_terminal_palette_set_property;
}

static void
foundry_terminal_palette_init (FoundryTerminalPalette *self)
{
}

FoundryTerminalPalette *
_foundry_terminal_palette_new (GKeyFile    *key_file,
                               const char  *group,
                               GError     **error)
{
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "Todo");
  return NULL;
}
