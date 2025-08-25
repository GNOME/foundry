/* foundry-tweaks-provider.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-tweaks-provider-private.h"
#include "foundry-util.h"

G_DEFINE_ABSTRACT_TYPE (FoundryTweaksProvider, foundry_tweaks_provider, FOUNDRY_TYPE_CONTEXTUAL)

static void
foundry_tweaks_provider_class_init (FoundryTweaksProviderClass *klass)
{
}

static void
foundry_tweaks_provider_init (FoundryTweaksProvider *self)
{
}

DexFuture *
_foundry_tweaks_provider_load (FoundryTweaksProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TWEAKS_PROVIDER (self));

  if (FOUNDRY_TWEAKS_PROVIDER_GET_CLASS (self)->load)
    return FOUNDRY_TWEAKS_PROVIDER_GET_CLASS (self)->load (self);

  return dex_future_new_true ();
}

DexFuture *
_foundry_tweaks_provider_unload (FoundryTweaksProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TWEAKS_PROVIDER (self));

  if (FOUNDRY_TWEAKS_PROVIDER_GET_CLASS (self)->unload)
    return FOUNDRY_TWEAKS_PROVIDER_GET_CLASS (self)->unload (self);

  return dex_future_new_true ();
}
