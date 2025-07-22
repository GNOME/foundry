/* foundry-source-buffer-addin.h
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

#pragma once

#include "foundry-contextual.h"
#include "foundry-source-buffer.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_SOURCE_BUFFER_ADDIN (foundry_source_buffer_addin_get_type())

FOUNDRY_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (FoundrySourceBufferAddin, foundry_source_buffer_addin, FOUNDRY, SOURCE_BUFFER_ADDIN, FoundryContextual)

struct _FoundrySourceBufferAddinClass
{
  FoundryContextualClass parent_class;

  DexFuture *(*load)   (FoundrySourceBufferAddin *self);
  DexFuture *(*unload) (FoundrySourceBufferAddin *self);

  /*< private >*/
  gpointer _reserved[8];
};

FOUNDRY_AVAILABLE_IN_ALL
FoundrySourceBuffer *foundry_source_buffer_addin_get_buffer (FoundrySourceBufferAddin *self);

G_END_DECLS
