/* plugin-meson-test-provider.c
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

#include "plugin-meson-test-provider.h"

struct _PluginMesonTestProvider
{
  FoundryTestProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginMesonTestProvider, plugin_meson_test_provider, FOUNDRY_TYPE_TEST_PROVIDER)

static DexFuture *
plugin_meson_test_provider_load (FoundryTestProvider *provider)
{
  PluginMesonTestProvider *self = (PluginMesonTestProvider *)provider;

  g_assert (PLUGIN_IS_MESON_TEST_PROVIDER (self));

  return dex_future_new_true ();
}

static DexFuture *
plugin_meson_test_provider_unload (FoundryTestProvider *provider)
{
  PluginMesonTestProvider *self = (PluginMesonTestProvider *)provider;

  g_assert (PLUGIN_IS_MESON_TEST_PROVIDER (self));

  return dex_future_new_true ();
}

static DexFuture *
plugin_meson_test_provider_list_tests (FoundryTestProvider *provider)
{
  PluginMesonTestProvider *self = (PluginMesonTestProvider *)provider;

  g_assert (PLUGIN_IS_MESON_TEST_PROVIDER (self));

  return dex_future_new_take_object (g_list_store_new (FOUNDRY_TYPE_TEST));
}

static void
plugin_meson_test_provider_class_init (PluginMesonTestProviderClass *klass)
{
  FoundryTestProviderClass *test_provider_class = FOUNDRY_TEST_PROVIDER_CLASS (klass);

  test_provider_class->load = plugin_meson_test_provider_load;
  test_provider_class->unload = plugin_meson_test_provider_unload;
  test_provider_class->list_tests = plugin_meson_test_provider_list_tests;
}

static void
plugin_meson_test_provider_init (PluginMesonTestProvider *self)
{
}
