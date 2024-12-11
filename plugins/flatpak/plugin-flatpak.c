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

typedef struct _ListRefs
{
  GPtrArray           *refs;
  FlatpakInstallation *installation;
  FlatpakQueryFlags    flags;
} ListRefs;

static void
list_refs_finalize (gpointer data)
{
  ListRefs *state = data;

  g_clear_pointer (&state->refs, g_ptr_array_unref);
  g_clear_object (&state->installation);
}

static void
list_refs_unref (ListRefs *state)
{
  g_atomic_rc_box_release_full (state, list_refs_finalize);
}

static ListRefs *
list_refs_ref (ListRefs *state)
{
  return g_atomic_rc_box_acquire (state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ListRefs, list_refs_unref)

static DexFuture *
plugin_flatpak_installation_list_refs_cb (gpointer user_data)
{
  ListRefs *state = user_data;
  g_autoptr(GPtrArray) remotes = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (FLATPAK_IS_INSTALLATION (state->installation));
  g_assert (state->refs != NULL);

  if (!(remotes = flatpak_installation_list_remotes (state->installation, NULL, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < remotes->len; i++)
    {
      g_autoptr(GPtrArray) refs = NULL;
      FlatpakRemote *remote = g_ptr_array_index (remotes, i);
      const char *name = flatpak_remote_get_name (remote);

      refs = flatpak_installation_list_remote_refs_sync_full (state->installation, name, state->flags, NULL, NULL);

      if (refs == NULL)
        continue;

      for (guint j = 0; j < refs->len; j++)
        {
          FlatpakRef *ref = g_ptr_array_index (refs, j);

          g_ptr_array_add (state->refs, g_object_ref (ref));
        }
    }

  return dex_future_new_take_boxed (G_TYPE_PTR_ARRAY, g_steal_pointer (&state->refs));
}

DexFuture *
plugin_flatpak_installation_list_refs (FlatpakInstallation *installation,
                                       FlatpakQueryFlags    flags)
{
  g_autoptr(ListRefs) state = NULL;

  dex_return_error_if_fail (FLATPAK_IS_INSTALLATION (installation));

  state = g_atomic_rc_box_new0 (ListRefs);
  state->refs = g_ptr_array_new_with_free_func (g_object_unref);
  state->installation = g_object_ref (installation);
  state->flags = flags;

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              plugin_flatpak_installation_list_refs_cb,
                              list_refs_ref (state),
                              (GDestroyNotify) list_refs_unref);
}
