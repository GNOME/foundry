/* test-flatpak-builder-manifest.c
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

#include <foundry.h>

#include "plugins/flatpak/builder/plugin-flatpak-manifest.h"
#include "plugins/flatpak/builder/plugin-flatpak-manifest-loader.h"

#include "test-util.h"

static void
test_builder_manifest_fiber (void)
{
  g_autoptr(GFile) srcdir = g_file_new_for_path (g_getenv ("G_TEST_SRCDIR"));
  g_autoptr(GFile) dir = g_file_get_child (srcdir, "test-manifests");

  static const char *files[] = {
    "gnome-builder/org.gnome.Builder.Devel.json",
  };

  for (guint i = 0; i < G_N_ELEMENTS (files); i++)
    {
      g_autoptr(PluginFlatpakManifestLoader) loader = NULL;
      g_autoptr(PluginFlatpakManifest) manifest = NULL;
      g_autoptr(GFile) file = g_file_get_child (dir, files[i]);
      g_autoptr(GError) error = NULL;

      loader = plugin_flatpak_manifest_loader_new (file);

      manifest = dex_await_object (plugin_flatpak_manifest_loader_load (loader), &error);
      g_assert_no_error (error);
      g_assert (PLUGIN_IS_FLATPAK_MANIFEST (manifest));
    }
}

static void
test_builder_manifest (void)
{
  test_from_fiber (test_builder_manifest_fiber);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/Plugins/Flatpak/Builder/Manifest", test_builder_manifest);
  return g_test_run ();
}
