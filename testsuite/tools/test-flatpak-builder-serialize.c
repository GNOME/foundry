/* test-flatpak-builder-serialize.c
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

#include <json-glib/json-glib.h>

#include <foundry.h>

#include "foundry-flatpak-manifest-loader.h"
#include "foundry-flatpak-serializable-private.h"

static char *manifest_filename;

static DexFuture *
serialize_fiber (gpointer data)
{
  GMainLoop *main_loop = data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = g_file_new_for_path (manifest_filename);
  g_autoptr(FoundryFlatpakManifestLoader) loader = NULL;
  g_autoptr(FoundryFlatpakManifest) manifest = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(JsonGenerator) generator = NULL;
  g_autofree char *contents = NULL;
  gsize len;

  loader = foundry_flatpak_manifest_loader_new (file);
  manifest = dex_await_object (foundry_flatpak_manifest_loader_load (loader), &error);

  if (error != NULL)
    {
      g_printerr ("Failed to load manifest: %s\n", error->message);
      g_main_loop_quit (main_loop);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  g_assert_nonnull (manifest);
  g_assert_true (FOUNDRY_IS_FLATPAK_MANIFEST (manifest));

  node = _foundry_flatpak_serializable_serialize (FOUNDRY_FLATPAK_SERIALIZABLE (manifest));

  if (node == NULL)
    {
      g_printerr ("Failed to serialize manifest\n");
      g_main_loop_quit (main_loop);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to serialize manifest");
    }

  generator = json_generator_new ();
  json_generator_set_root (generator, node);
  json_generator_set_pretty (generator, TRUE);
  json_generator_set_indent (generator, 4);

  contents = json_generator_to_data (generator, &len);

  g_print ("%s\n", contents);

  g_main_loop_quit (main_loop);

  return dex_future_new_true ();
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);

  if (argc != 2)
    {
      g_printerr ("usage: %s MANIFEST_FILE\n", argv[0]);
      return 1;
    }

  manifest_filename = argv[1];

  dex_init ();

  dex_future_disown (dex_scheduler_spawn (NULL, 0, serialize_fiber, main_loop, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
