/* foundry-unknown-request.c
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
#include "foundry-dap-unknown-request.h"

struct _FoundryDapUnknownRequest
{
  FoundryDapRequest parent_instance;
};

struct _FoundryDapUnknownRequestClass
{
  FoundryDapRequestClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryDapUnknownRequest, foundry_dap_unknown_request, FOUNDRY_TYPE_DAP_REQUEST)

static void
foundry_dap_unknown_request_class_init (FoundryDapUnknownRequestClass *klass)
{
}

static void
foundry_dap_unknown_request_init (FoundryDapUnknownRequest *self)
{
}
