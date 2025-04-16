/* foundry-unknown-response.c
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

#include "foundry-dap-response-private.h"
#include "foundry-dap-unknown-response.h"

struct _FoundryDapUnknownResponse
{
  FoundryDapResponse parent_instance;
};

struct _FoundryDapUnknownResponseClass
{
  FoundryDapResponseClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryDapUnknownResponse, foundry_dap_unknown_response, FOUNDRY_TYPE_DAP_RESPONSE)

static void
foundry_dap_unknown_response_class_init (FoundryDapUnknownResponseClass *klass)
{
}

static void
foundry_dap_unknown_response_init (FoundryDapUnknownResponse *self)
{
}
