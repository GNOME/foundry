/* foundry-code-action-provider.c
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

#include "foundry-code-action-provider.h"

G_DEFINE_ABSTRACT_TYPE (FoundryCodeActionProvider, foundry_code_action_provider, FOUNDRY_TYPE_CONTEXTUAL)

static void
foundry_code_action_provider_class_init (FoundryCodeActionProviderClass *klass)
{
}

static void
foundry_code_action_provider_init (FoundryCodeActionProvider *self)
{
}
