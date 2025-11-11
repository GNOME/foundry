/* test-read-all-bytes.c
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

#include <fcntl.h>
#include <unistd.h>

#include <foundry.h>

#include "foundry-util-private.h"
#include "test-util.h"

static void
test_read_all_bytes_fiber (void)
{
  g_autoptr(GFile) srcdir = g_file_new_for_path (g_getenv ("G_TEST_SRCDIR"));
  g_autoptr(GFile) test_file = g_file_get_child (srcdir, "test-read-all-bytes");
  g_autoptr(GFile) data_file = g_file_get_child (test_file, "test-data.txt");
  g_autofree char *path = NULL;
  g_autofree char *expected_contents = NULL;
  gsize expected_length = 0;
  g_autoptr(GBytes) expected_bytes = NULL;
  g_autoptr(GBytes) actual_bytes = NULL;
  g_autoptr(GError) error = NULL;
  int fd = -1;

  path = g_file_get_path (data_file);
  g_assert_nonnull (path);

  /* Read the file using g_file_get_contents() for comparison */
  g_assert_true (g_file_get_contents (path, &expected_contents, &expected_length, &error));
  g_assert_no_error (error);
  g_assert_nonnull (expected_contents);
  g_assert_cmpint (expected_length, >, 0);

  expected_bytes = g_bytes_new (expected_contents, expected_length);

  /* Open the file descriptor and read using _foundry_read_all_bytes() */
  fd = open (path, O_RDONLY);
  g_assert_cmpint (fd, >=, 0);

  actual_bytes = dex_await_boxed (_foundry_read_all_bytes (fd), &error);
  g_assert_no_error (error);
  g_assert_nonnull (actual_bytes);

  close (fd);

  /* Compare the bytes */
  g_assert_cmpint (g_bytes_get_size (expected_bytes), ==, g_bytes_get_size (actual_bytes));
  g_assert_true (g_bytes_equal (expected_bytes, actual_bytes));
}

static void
test_read_all_bytes (void)
{
  test_from_fiber (test_read_all_bytes_fiber);
}

int
main (int argc,
      char *argv[])
{
  const char *srcdir = g_getenv ("G_TEST_SRCDIR");

  g_assert_nonnull (srcdir);

  dex_init ();

  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/Util/read_all_bytes", test_read_all_bytes);

  return g_test_run ();
}
