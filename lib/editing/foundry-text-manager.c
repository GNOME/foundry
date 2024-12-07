/* foundry-text-manager.c
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

#include "foundry-text-manager.h"
#include "foundry-service-private.h"

struct _FoundryTextManager
{
  FoundryService parent_instance;
};

struct _FoundryTextManagerClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryTextManager, foundry_text_manager, FOUNDRY_TYPE_SERVICE)

static void
foundry_text_manager_finalize (GObject *object)
{
  FoundryTextManager *self = (FoundryTextManager *)object;

  G_OBJECT_CLASS (foundry_text_manager_parent_class)->finalize (object);
}

static void
foundry_text_manager_class_init (FoundryTextManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_text_manager_finalize;
}

static void
foundry_text_manager_init (FoundryTextManager *self)
{
}

/**
 * foundry_text_manager_load:
 * @self: a #FoundryTextManager
 * @file: a #GFile
 *
 * Loads the file as a text buffer.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Foundry.TextBuffer].
 */
DexFuture *
foundry_text_manager_load (FoundryTextManager *self,
                           GFile              *file)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEXT_MANAGER (self));
  dex_return_error_if_fail (G_IS_FILE (file));

  return NULL;
}
