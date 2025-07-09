/* test-ollama.c
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

#include "plugins/ollama/plugin-ollama-client.h"
#include "plugins/ollama/plugin-ollama-model.h"

#include "test-util.h"

static void
test_list_models_fiber (void)
{
  g_autoptr(PluginOllamaClient) client = NULL;
  g_autoptr(GListModel) models = NULL;
  g_autoptr(GError) error = NULL;
  guint n_items;

  g_message ("Creating client");
  client = plugin_ollama_client_new (NULL);
  g_assert_nonnull (client);
  g_assert_true (PLUGIN_IS_OLLAMA_CLIENT (client));

  g_message ("Querying list of models");
  models = dex_await_object (plugin_ollama_client_list_models (client), &error);
  g_assert_no_error (error);
  g_assert_nonnull (models);
  g_assert_true (G_IS_LIST_MODEL (models));

  n_items = g_list_model_get_n_items (models);
  g_message ("%u models found. Checking types.", n_items);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PluginOllamaModel) model = g_list_model_get_item (models, i);
      g_autofree char *name = NULL;

      g_assert_true (PLUGIN_IS_OLLAMA_MODEL (model));

      name = plugin_ollama_model_dup_name (model);
      g_message ("Found model named `%s`", name);
    }
}

static void
test_list_models (void)
{
  test_from_fiber (test_list_models_fiber);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Plugins/Ollama/Client/list_models", test_list_models);
  return g_test_run ();
}
