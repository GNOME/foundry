/* foundry-dap-request.c
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

#include "foundry-dap-request-private.h"

G_DEFINE_TYPE (FoundryDapRequest, foundry_dap_request, FOUNDRY_TYPE_DAP_PROTOCOL_MESSAGE)

static void
foundry_dap_request_finalize (GObject *object)
{
  FoundryDapRequest *self = (FoundryDapRequest *)object;

  G_OBJECT_CLASS (foundry_dap_request_parent_class)->finalize (object);
}

static void
foundry_dap_request_class_init (FoundryDapRequestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_dap_request_finalize;
}

static void
foundry_dap_request_init (FoundryDapRequest *self)
{
}
