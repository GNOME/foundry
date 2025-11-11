/* plugin-ctags-symbol.c
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

#include "plugin-ctags-file.h"
#include "plugin-ctags-symbol.h"

struct _PluginCtagsSymbol
{
  FoundrySymbol parent_instance;
  PluginCtagsFile *file;
  PluginCtagsMatch match;
};

G_DEFINE_FINAL_TYPE (PluginCtagsSymbol, plugin_ctags_symbol, FOUNDRY_TYPE_SYMBOL)

static void
plugin_ctags_symbol_finalize (GObject *object)
{
  PluginCtagsSymbol *self = PLUGIN_CTAGS_SYMBOL (object);

  g_clear_object (&self->file);

  G_OBJECT_CLASS (plugin_ctags_symbol_parent_class)->finalize (object);
}

static char *
plugin_ctags_symbol_dup_name (FoundrySymbol *symbol)
{
  PluginCtagsSymbol *self = PLUGIN_CTAGS_SYMBOL (symbol);

  return g_strndup (self->match.name, self->match.name_len);
}

static void
plugin_ctags_symbol_class_init (PluginCtagsSymbolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySymbolClass *symbol_class = FOUNDRY_SYMBOL_CLASS (klass);

  object_class->finalize = plugin_ctags_symbol_finalize;

  symbol_class->dup_name = plugin_ctags_symbol_dup_name;
}

static void
plugin_ctags_symbol_init (PluginCtagsSymbol *self)
{
}

PluginCtagsSymbol *
plugin_ctags_symbol_new (PluginCtagsFile        *file,
                         const PluginCtagsMatch *match)
{
  PluginCtagsSymbol *self;

  g_return_val_if_fail (PLUGIN_IS_CTAGS_FILE (file), NULL);
  g_return_val_if_fail (match != NULL, NULL);

  self = g_object_new (PLUGIN_TYPE_CTAGS_SYMBOL, NULL);
  self->file = g_object_ref (file);
  self->match = *match;

  return self;
}
