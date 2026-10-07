/* test-flatpak-builder-manifest.c
 *
 * Copyright 2025 Christian Hergert <christian@sourceandstack.com>
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

#include "foundry-flatpak-manifest-loader-private.h"

#include "test-util.h"

static void
test_builder_manifest_multiple_roots_fiber (void)
{
  g_autoptr(GFile) srcdir = g_file_new_for_path (g_getenv ("G_TEST_SRCDIR"));
  g_autoptr(GFile) dir = g_file_get_child (srcdir, "test-manifests");
  g_autoptr(GFile) file = NULL;
  g_autoptr(JsonNode) root = NULL;
  g_autoptr(GError) error = NULL;

  file = g_file_get_child (dir,
                           "multiple-roots-failure/org.gnome.Foundry.Test.json");
  root = dex_await_boxed (_foundry_flatpak_manifest_load_file_as_json (file), &error);

  g_assert_no_error (error);
  g_assert_true (JSON_NODE_HOLDS_ARRAY (root));
  g_assert_true (json_node_is_immutable (root));
}

static void
test_builder_manifest_multiple_roots (void)
{
  test_from_fiber (test_builder_manifest_multiple_roots_fiber);
}

static void
test_builder_manifest_fiber (void)
{
  g_autoptr(GFile) srcdir = g_file_new_for_path (g_getenv ("G_TEST_SRCDIR"));
  g_autoptr(GFile) dir = g_file_get_child (srcdir, "test-manifests");

  const struct {
    const char *path;
    GQuark domain;
    guint code;
  } files[] = {
    { "gnome-builder/org.gnome.Builder.Devel.json" },
    { "simple/org.gnome.Foundry.Test.yaml" },
    { "jump-out-of-root-failure/org.gnome.foundry.testsuite.escape.json", G_IO_ERROR, G_IO_ERROR_NOT_FOUND },
    { "multiple-roots-failure/org.gnome.Foundry.Test.json", G_IO_ERROR, G_IO_ERROR_INVALID_DATA },
  };

  for (guint i = 0; i < G_N_ELEMENTS (files); i++)
    {
      g_autoptr(FoundryFlatpakManifestLoader) loader = NULL;
      g_autoptr(FoundryFlatpakManifest) manifest = NULL;
      g_autoptr(GFile) file = g_file_get_child (dir, files[i].path);
      g_autoptr(GError) error = NULL;

      loader = foundry_flatpak_manifest_loader_new (file);
      manifest = dex_await_object (foundry_flatpak_manifest_loader_load (loader), &error);

      if (files[i].domain)
        {
          g_assert_error (error, files[i].domain, files[i].code);
          g_assert_null (manifest);
        }
      else
        {
          g_assert_no_error (error);
          g_assert (FOUNDRY_IS_FLATPAK_MANIFEST (manifest));
        }
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
  g_test_add_func ("/Foundry/Plugins/Flatpak/Builder/Manifest-multiple-roots",
                   test_builder_manifest_multiple_roots);
  return g_test_run ();
}
