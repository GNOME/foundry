/* foundry-gtk-tweak-provider.c
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

#include <glib/gi18n-lib.h>

#include "foundry-gtk-tweak-provider-private.h"

struct _FoundryGtkTweakProvider
{
  FoundryTweakProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryGtkTweakProvider, foundry_gtk_tweak_provider, FOUNDRY_TYPE_TWEAK_PROVIDER)

static const FoundryTweakInfo page_infos[] = {
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .availability = FOUNDRY_TWEAK_AVAILABILITY_ANY,
    .subpath = "/languages/",
    .title = N_("Programming Languages"),
    .icon_name = "text-x-js-symbolic",
  },
};

static const FoundryTweakInfo language_infos[] = {
  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .availability = FOUNDRY_TWEAK_AVAILABILITY_ANY,
    .subpath = "/languages/@language@/formatting/",
    .title = N_("Indentation & Formatting"),
    .sort_key = "001",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .availability = FOUNDRY_TWEAK_AVAILABILITY_ANY,
    .subpath = "/languages/@language@/indentation/implicit-trailing-newline",
    .title = N_("Insert Trailing Newline"),
    .subtitle = N_("Ensure files end with a newline"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = "app.devsuite.foundry.text",
      .setting.path = "/app/devsuite/foundry/text/@language@/",
      .setting.key = "implicit-trailing-newline",
    },
  },

  {
    .type = FOUNDRY_TWEAK_TYPE_GROUP,
    .availability = FOUNDRY_TWEAK_AVAILABILITY_ANY,
    .subpath = "/languages/@language@/indentation/",
    .sort_key = "002",
  },
  {
    .type = FOUNDRY_TWEAK_TYPE_SWITCH,
    .availability = FOUNDRY_TWEAK_AVAILABILITY_ANY,
    .subpath = "/languages/@language@/indentation/auto-indent",
    .title = N_("Auto Indent"),
    .subtitle = N_("Automatically indent while you type"),
    .source = &(FoundryTweakSource) {
      .type = FOUNDRY_TWEAK_SOURCE_TYPE_SETTING,
      .setting.schema_id = "app.devsuite.foundry.text",
      .setting.path = "/app/devsuite/foundry/text/@language@/",
      .setting.key = "auto-indent",
    },
  },
};

static DexFuture *
foundry_gtk_tweak_provider_load (FoundryTweakProvider *provider)
{
  g_assert (FOUNDRY_IS_GTK_TWEAK_PROVIDER (provider));

  return dex_future_new_true ();
}

static void
foundry_gtk_tweak_provider_class_init (FoundryGtkTweakProviderClass *klass)
{
  FoundryTweakProviderClass *tweak_provider_class = FOUNDRY_TWEAK_PROVIDER_CLASS (klass);

  tweak_provider_class->load = foundry_gtk_tweak_provider_load;
}

static void
foundry_gtk_tweak_provider_init (FoundryGtkTweakProvider *self)
{
}
