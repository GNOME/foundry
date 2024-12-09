/* test-settings.c
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

#include <foundry.h>

#include "test-util.h"

static void
test_settings_fiber (void)
{
  const char *srcdir = g_getenv ("G_TEST_SRCDIR");
  const char *builddir = g_getenv ("G_TEST_BUILDDIR");
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundrySettings) settings = NULL;
  g_autofree char *foundry_dir = NULL;
  g_autofree char *project_dir = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *str1 = NULL;

  g_assert_nonnull (builddir);
  g_assert_nonnull (srcdir);

  project_dir = g_build_filename (srcdir, "test-settings", NULL);
  foundry_dir = g_build_filename (builddir, "test-settings-data", ".foundry", NULL);

  rm_rf (foundry_dir);

  g_mkdir_with_parents (foundry_dir, 0770);

  g_assert_true (g_file_test (project_dir, G_FILE_TEST_IS_DIR));
  g_assert_true (g_file_test (foundry_dir, G_FILE_TEST_IS_DIR));

  context = dex_await_object (foundry_context_new (foundry_dir, project_dir, 0, NULL), &error);
  g_assert_no_error (error);
  g_assert_nonnull (context);

  settings = foundry_context_load_settings (context, "app.devsuite.foundry.project", NULL);
  foundry_settings_set_string (settings, "config-id", "my-config");
  str1 = foundry_settings_get_string (settings, "config-id");
  g_assert_cmpstr (str1, ==, "my-config");
}

static void
test_settings (void)
{
  test_from_fiber (test_settings_fiber);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/Settings/basic", test_settings);
  return g_test_run ();
}
