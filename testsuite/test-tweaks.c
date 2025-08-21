/* test-tweaks.c
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
test_tweaks_path_fiber (void)
{
  g_autoptr(FoundryTweaksPath) root = foundry_tweaks_path_new (FOUNDRY_TWEAKS_PATH_MODE_DEFAULTS, NULL);
  g_assert_true (foundry_tweaks_path_is_root (root));

  {
    g_autoptr(FoundryTweaksPath) basic = foundry_tweaks_path_new (FOUNDRY_TWEAKS_PATH_MODE_DEFAULTS,
                                                                  FOUNDRY_STRV_INIT ("basic"));
    g_assert_true (foundry_tweaks_path_has_prefix (basic, root));
    g_assert_false (foundry_tweaks_path_equal (basic, root));
    g_assert_false (foundry_tweaks_path_has_prefix (root, basic));

    g_assert_false (FOUNDRY_TWEAKS_PATH_FOR_PROJECT (basic));
    g_assert_false (FOUNDRY_TWEAKS_PATH_FOR_USER (basic));
    g_assert_true (FOUNDRY_TWEAKS_PATH_FOR_DEFAULTS (basic));

    {
      g_autoptr(FoundryTweaksPath) basic2 = foundry_tweaks_path_push (root, "basic");
      g_assert_true (foundry_tweaks_path_has_prefix (basic2, root));
      g_assert_false (foundry_tweaks_path_has_prefix (basic2, basic));
      g_assert_true (foundry_tweaks_path_equal (basic2, basic));

      {
        g_autoptr(FoundryTweaksPath) root2 = foundry_tweaks_path_pop (basic2);
        g_assert_nonnull (root2);
        g_assert_true (foundry_tweaks_path_equal (root2, root));
        g_assert_false (foundry_tweaks_path_equal (root2, basic2));
      }
    }

    {
      g_autoptr(FoundryTweaksPath) basic2 = foundry_tweaks_path_new (FOUNDRY_TWEAKS_PATH_MODE_PROJECT,
                                                                     FOUNDRY_STRV_INIT ("basic"));
      g_assert_false (foundry_tweaks_path_equal (basic2, basic));
      g_assert_false (foundry_tweaks_path_has_prefix (basic2, root));

      g_assert_true (FOUNDRY_TWEAKS_PATH_FOR_PROJECT (basic2));
      g_assert_false (FOUNDRY_TWEAKS_PATH_FOR_USER (basic2));
      g_assert_false (FOUNDRY_TWEAKS_PATH_FOR_DEFAULTS (basic2));
    }
  }

}

static void
test_tweaks_path (void)
{
  test_from_fiber (test_tweaks_path_fiber);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/Tweaks/Path/basic", test_tweaks_path);
  return g_test_run ();
}
