/* foundry-terminal.c
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

#include "foundry-terminal.h"
#include "foundry-terminal-palette-private.h"

typedef struct
{
  FoundryTerminalPalette *palette;
} FoundryTerminalPrivate;

enum {
  PROP_0,
  PROP_PALETTE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (FoundryTerminal, foundry_terminal, VTE_TYPE_TERMINAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_terminal_finalize (GObject *object)
{
  FoundryTerminal *self = (FoundryTerminal *)object;
  FoundryTerminalPrivate *priv = foundry_terminal_get_instance_private (self);

  g_clear_object (&priv->palette);

  G_OBJECT_CLASS (foundry_terminal_parent_class)->finalize (object);
}

static void
foundry_terminal_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  FoundryTerminal *self = FOUNDRY_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      g_value_set_object (value, foundry_terminal_get_palette (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_terminal_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  FoundryTerminal *self = FOUNDRY_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      foundry_terminal_set_palette (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_terminal_class_init (FoundryTerminalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_terminal_finalize;
  object_class->get_property = foundry_terminal_get_property;
  object_class->set_property = foundry_terminal_set_property;

  properties[PROP_PALETTE] =
    g_param_spec_object ("palette", NULL, NULL,
                         FOUNDRY_TYPE_TERMINAL_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_terminal_init (FoundryTerminal *self)
{
}

GtkWidget *
foundry_terminal_new (void)
{
  return g_object_new (FOUNDRY_TYPE_TERMINAL, NULL);
}

/**
 * foundry_terminal_get_palette:
 * @self: a [class@FoundryGtk.Terminal]
 *
 * Returns: (transfer none) (nullable):
 */
FoundryTerminalPalette *
foundry_terminal_get_palette (FoundryTerminal *self)
{
  FoundryTerminalPrivate *priv = foundry_terminal_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_TERMINAL (self), NULL);

  return priv->palette;
}

void
foundry_terminal_set_palette (FoundryTerminal        *self,
                              FoundryTerminalPalette *palette)
{
  FoundryTerminalPrivate *priv = foundry_terminal_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_TERMINAL (self));
  g_return_if_fail (!palette || FOUNDRY_IS_TERMINAL_PALETTE (palette));

  if (g_set_object (&priv->palette, palette))
    {
      if (priv->palette != NULL)
        _foundry_terminal_palette_apply (priv->palette, VTE_TERMINAL (self));
      else
        vte_terminal_set_default_colors (VTE_TERMINAL (self));

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
    }
}
