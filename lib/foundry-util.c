/* foundry-util.c
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

#include <errno.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <gio/gio.h>

#include "line-reader-private.h"

#include "foundry-triplet.h"
#include "foundry-util-private.h"

static char **
get_environ_from_stdout (GSubprocess *subprocess)
{
  g_autofree char *stdout_buf = NULL;

  if (g_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, NULL))
    {
      g_autoptr(GPtrArray) env = g_ptr_array_new_with_free_func (g_free);
      LineReader reader;
      gsize line_len;
      char *line;

      line_reader_init (&reader, stdout_buf, -1);

      while ((line = line_reader_next (&reader, &line_len)))
        {
          line[line_len] = 0;

          if (!g_ascii_isalpha (*line) && *line != '_')
            continue;

          for (const char *iter = line; *iter; iter = g_utf8_next_char (iter))
            {
              if (*iter == '=')
                {
                  g_ptr_array_add (env, g_strdup (line));
                  break;
                }

              if (!g_ascii_isalnum (*iter) && *iter != '_')
                break;
            }
        }

      if (env->len > 0)
        {
          g_ptr_array_add (env, NULL);
          return (char **)g_ptr_array_free (g_steal_pointer (&env), FALSE);
        }
    }

  return NULL;
}

gboolean
_foundry_in_container (void)
{
  static gsize initialized;
  static gboolean in_container;

  if (g_once_init_enter (&initialized))
    {
      in_container = g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS) ||
                     g_file_test ("/var/run/.containerenv", G_FILE_TEST_EXISTS);
      g_once_init_leave (&initialized, TRUE);
    }

  return in_container;
}

const char * const *
_foundry_host_environ (void)
{
  static char **host_environ;

  if (host_environ == NULL)
    {
      if (_foundry_in_container ())
        {
          g_autoptr(GSubprocessLauncher) launcher = NULL;
          g_autoptr(GSubprocess) subprocess = NULL;
          g_autoptr(GError) error = NULL;

          launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
          subprocess = g_subprocess_launcher_spawn (launcher, &error,
                                                    "flatpak-spawn", "--host", "printenv", NULL);
          if (subprocess != NULL)
            host_environ = get_environ_from_stdout (subprocess);
        }

      if (host_environ == NULL)
        host_environ = g_get_environ ();
    }

  return (const char * const *)host_environ;
}

char *
_foundry_create_host_triplet (const char *arch,
                              const char *kernel,
                              const char *system)
{
  if (arch == NULL || kernel == NULL)
    return g_strdup (_foundry_get_system_type ());
  else if (system == NULL)
    return g_strdup_printf ("%s-%s", arch, kernel);
  else
    return g_strdup_printf ("%s-%s-%s", arch, kernel, system);
}

const char *
_foundry_get_system_type (void)
{
  static char *system_type;
  g_autofree char *os_lower = NULL;
  const char *machine = NULL;
  struct utsname u;

  if (system_type != NULL)
    return system_type;

  if (uname (&u) < 0)
    return g_strdup ("unknown");

  os_lower = g_utf8_strdown (u.sysname, -1);

  /* config.sub doesn't accept amd64-OS */
  machine = strcmp (u.machine, "amd64") ? u.machine : "x86_64";

  /*
   * TODO: Clearly we want to discover "gnu", but that should be just fine
   *       for a default until we try to actually run on something non-gnu.
   *       Which seems unlikely at the moment. If you run FreeBSD, you can
   *       probably fix this for me :-) And while you're at it, make the
   *       uname() call more portable.
   */

#ifdef __GLIBC__
  system_type = g_strdup_printf ("%s-%s-%s", machine, os_lower, "gnu");
#else
  system_type = g_strdup_printf ("%s-%s", machine, os_lower);
#endif

  return system_type;
}

char *
_foundry_get_system_arch (void)
{
  static GHashTable *remap;
  const char *machine;
  struct utsname u;

  if (uname (&u) < 0)
    return g_strdup ("unknown");

  if (g_once_init_enter (&remap))
    {
      GHashTable *mapping;

      mapping = g_hash_table_new (g_str_hash, g_str_equal);
      g_hash_table_insert (mapping, (char *)"amd64", (char *)"x86_64");
      g_hash_table_insert (mapping, (char *)"armv7l", (char *)"aarch64");
      g_hash_table_insert (mapping, (char *)"i686", (char *)"i386");

      g_once_init_leave (&remap, mapping);
    }

  if (g_hash_table_lookup_extended (remap, u.machine, NULL, (gpointer *)&machine))
    return g_strdup (machine);
  else
    return g_strdup (u.machine);
}

void
_foundry_fd_write_all (int         fd,
                       const char *message,
                       gssize      to_write)
{
  const char *data = message;

  if (fd < 0)
    return;

  if (to_write < 0)
    to_write = strlen (message);

  while (to_write > 0)
    {
      gssize n_written;

      errno = 0;
      n_written = write (fd, data, to_write);

      if (n_written < 0)
        return;

      if (n_written == 0 && errno == EINTR)
        continue;

      to_write -= n_written;
      data += (gsize)n_written;
    }
}

typedef struct
{
  char *tmpdir;
  char *template_name;
  int mode;
} Mkdtemp;

static void
mkdtemp_free (Mkdtemp *state)
{
  g_clear_pointer (&state->tmpdir, g_free);
  g_clear_pointer (&state->template_name, g_free);
  g_free (state);
}

static DexFuture *
_foundry_mkdtemp_fiber (gpointer data)
{
  static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static const guint n_letters = sizeof letters - 1;

  Mkdtemp *state = data;
  g_autofree char *copy = g_strdup (state->template_name);
  char *XXXXXX = strstr (copy, "XXXXXX");

  if (XXXXXX == NULL)
    return dex_future_new_reject (G_FILE_ERROR,
                                  G_FILE_ERROR_INVAL,
                                  "Invalid template name %s",
                                  state->template_name);

  for (guint count = 0; count < 100; count++)
    {
      gint64 v = g_get_real_time () ^ g_random_int ();
      g_autofree char *path = NULL;

      XXXXXX[0] = letters[v % n_letters];
      v /= n_letters;
      XXXXXX[1] = letters[v % n_letters];
      v /= n_letters;
      XXXXXX[2] = letters[v % n_letters];
      v /= n_letters;
      XXXXXX[3] = letters[v % n_letters];
      v /= n_letters;
      XXXXXX[4] = letters[v % n_letters];
      v /= n_letters;
      XXXXXX[5] = letters[v % n_letters];

      path = g_build_filename (state->tmpdir, copy, NULL);

      errno = 0;

      if (g_mkdir (path, state->mode) == 0)
        return dex_future_new_for_string (g_steal_pointer (&path));

      if (errno != EEXIST)
        {
          int errsv = errno;
          return dex_future_new_reject (G_FILE_ERROR,
                                        g_file_error_from_errno (errsv),
                                        "%s",
                                        g_strerror (errsv));
        }
    }

  return dex_future_new_reject (G_FILE_ERROR,
                                G_FILE_ERROR_EXIST,
                                "%s",
                                g_strerror (EEXIST));
}

DexFuture *
_foundry_mkdtemp (const char *tmpdir,
                  const char *template_name)
{
  Mkdtemp *state;

  g_return_val_if_fail (tmpdir != NULL, NULL);
  g_return_val_if_fail (template_name != NULL, NULL);
  g_return_val_if_fail (strstr (template_name, "XXXXXX") != NULL, NULL);

  state = g_new0 (Mkdtemp, 1);
  state->tmpdir = g_strdup (tmpdir);
  state->template_name = g_strdup (template_name);
  state->mode = 0770;

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              _foundry_mkdtemp_fiber,
                              state,
                              (GDestroyNotify)mkdtemp_free);
}

const char *
foundry_get_default_arch (void)
{
  static const char *default_arch;

  if (g_once_init_enter (&default_arch))
    {
      g_autoptr(FoundryTriplet) triplet = foundry_triplet_new_from_system ();
      const char *value = g_intern_string (foundry_triplet_get_arch (triplet));
      g_once_init_leave (&default_arch, value);
    }

  return default_arch;
}

typedef struct _KeyFileNewFromFile
{
  GFile *file;
  GKeyFileFlags flags;
} KeyFileNewFromFile;

static void
key_file_new_from_file_free (KeyFileNewFromFile *state)
{
  g_clear_object (&state->file);
  g_free (state);
}

static DexFuture *
foundry_key_file_new_from_file_fiber (gpointer user_data)
{
  KeyFileNewFromFile *state = user_data;
  g_autoptr(GKeyFile) key_file = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;

  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));

  key_file = g_key_file_new ();

  if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (state->file), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!g_key_file_load_from_bytes (key_file, bytes, state->flags, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_boxed (G_TYPE_KEY_FILE, g_steal_pointer (&key_file));
}

DexFuture *
foundry_key_file_new_from_file (GFile         *file,
                                GKeyFileFlags  flags)
{
  KeyFileNewFromFile *state;

  dex_return_error_if_fail (G_IS_FILE (file));

  state = g_new0 (KeyFileNewFromFile, 1);
  state->file = g_object_ref (file);
  state->flags = flags;

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              foundry_key_file_new_from_file_fiber,
                              state,
                              (GDestroyNotify) key_file_new_from_file_free);
}
