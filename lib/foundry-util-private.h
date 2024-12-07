/* foundry-util-private.h
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

#pragma once

#include <libdex.h>

G_BEGIN_DECLS

#define FOUNDRY_STRV_INIT(...) ((const char * const[]) { __VA_ARGS__, NULL})

gboolean            _foundry_in_container        (void);
const char * const *_foundry_host_environ        (void);
char               *_foundry_create_host_triplet (const char *arch,
                                                  const char *kernel,
                                                  const char *system);
const char         *_foundry_get_system_type     (void);
char               *_foundry_get_system_arch     (void);
void                _foundry_fd_write_all        (int         fd,
                                                  const char *message,
                                                  gssize      to_write);
DexFuture          *_foundry_mkdtemp             (const char *tmpdir,
                                                  const char *template_name);

static inline gboolean
foundry_str_equal0 (const char *a,
                    const char *b)
{
  return a == b || g_strcmp0 (a, b) == 0;
}

static inline gboolean
foundry_str_empty0 (const char *str)
{
  return str == NULL || str[0] == 0;
}

G_GNUC_WARN_UNUSED_RESULT
static inline DexFuture *
foundry_future_all (GPtrArray *ar)
{
  g_assert (ar != NULL);
  g_assert (ar->len > 0);

  return dex_future_allv ((DexFuture **)ar->pdata, ar->len);
}

G_END_DECLS
