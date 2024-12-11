/* plugin-flatpak.c
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

#include <flatpak.h>

#include "plugin-flatpak.h"

static DexFuture *
plugin_flatpak_installation_new_system_fiber (gpointer user_data)
{
  FlatpakInstallation *installation;
  GError *error = NULL;

  if (!(installation = flatpak_installation_new_system (NULL, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));
  else
    return dex_future_new_take_object (g_steal_pointer (&installation));
}

DexFuture *
plugin_flatpak_installation_new_system (void)
{
  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              plugin_flatpak_installation_new_system_fiber,
                              NULL, NULL);
}

static DexFuture *
plugin_flatpak_installation_new_user_fiber (gpointer user_data)
{
  FlatpakInstallation *installation;
  GError *error = NULL;

  if (!(installation = flatpak_installation_new_user (NULL, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));
  else
    return dex_future_new_take_object (g_steal_pointer (&installation));
}

DexFuture *
plugin_flatpak_installation_new_user (void)
{
  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              plugin_flatpak_installation_new_user_fiber,
                              NULL, NULL);
}

typedef struct _NewForPath
{
  GFile *file;
  guint is_user : 1;
} NewForPath;

static void
new_for_path_free (NewForPath *state)
{
  g_clear_object (&state->file);
  g_free (state);
}

static DexFuture *
plugin_flatpak_installation_new_for_path_fiber (gpointer user_data)
{
  NewForPath *state = user_data;
  FlatpakInstallation *installation;
  GError *error = NULL;

  if (!(installation = flatpak_installation_new_for_path (state->file, state->is_user, NULL, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));
  else
    return dex_future_new_take_object (g_steal_pointer (&installation));
}

DexFuture *
plugin_flatpak_installation_new_for_path (GFile    *path,
                                          gboolean  user)
{
  NewForPath *state;

  dex_return_error_if_fail (G_IS_FILE (path));

  state = g_new0 (NewForPath, 1);
  state->file = g_object_ref (path);
  state->is_user = !!user;

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              plugin_flatpak_installation_new_for_path_fiber,
                              state,
                              (GDestroyNotify) new_for_path_free);
}

DexFuture *
plugin_flatpak_installation_new_private (FoundryContext *context)
{
  g_autoptr(FoundrySettings) settings = NULL;
  g_autofree char *path = NULL;
  g_autoptr(GFile) file = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));

  settings = foundry_context_load_settings (context, "app.devsuite.foundry.flatpak", NULL);
  path = foundry_settings_get_string (settings, "private-installation");

  if (foundry_str_empty0 (path))
    {
      g_free (path);
      path = g_build_filename (g_get_home_dir (), "Projects", ".foundry-flatpak", NULL);
    }

  file = g_file_new_for_path (path);

  return plugin_flatpak_installation_new_for_path (file, TRUE);
}
