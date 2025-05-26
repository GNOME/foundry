/* foundry-gtk-init.c
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

#include "foundry-gtk-init.h"
#include "foundry-gtk-resources.h"
#include "foundry-source-buffer.h"
#include "foundry-source-buffer-provider-private.h"

#include "gconstructor.h"

static void
_foundry_gtk_init_once (void)
{
  g_resources_register (_foundry_gtk_get_resource ());

  dex_future_disown (foundry_init ());

  g_type_ensure (FOUNDRY_TYPE_SOURCE_BUFFER);
  g_type_ensure (FOUNDRY_TYPE_SOURCE_BUFFER_PROVIDER);
}

void
foundry_gtk_init (void)
{
  static gsize initialized;

  if (g_once_init_enter (&initialized))
    {
      _foundry_gtk_init_once ();
      g_once_init_leave (&initialized, TRUE);
    }
}

G_DEFINE_CONSTRUCTOR (foundry_gtk_init_ctor)

static void
foundry_gtk_init_ctor (void)
{
  foundry_gtk_init ();
}
