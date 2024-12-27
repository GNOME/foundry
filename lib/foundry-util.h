/* foundry-util.h
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

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_STRV_INIT(...) ((const char * const[]) { __VA_ARGS__, NULL})

FOUNDRY_AVAILABLE_IN_ALL
const char *foundry_get_default_arch       (void);
FOUNDRY_AVAILABLE_IN_ALL
DexFuture  *foundry_key_file_new_from_file (GFile         *file,
                                            GKeyFileFlags  flags) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_ALL
DexFuture  *foundry_file_test              (const char    *path,
                                            GFileTest      test) G_GNUC_WARN_UNUSED_RESULT;

#ifndef __GI_SCANNER__

typedef struct _FoundryPair
{
  GObject *first;
  GObject *second;
} FoundryPair;

static inline FoundryPair *
foundry_pair_new (gpointer first,
                  gpointer second)
{
  FoundryPair *pair = g_new0 (FoundryPair, 1);
  g_set_object (&pair->first, first);
  g_set_object (&pair->second, second);
  return pair;
}

static inline void
foundry_pair_free (FoundryPair *pair)
{
  g_clear_object (&pair->first);
  g_clear_object (&pair->second);
  g_free (pair);
}

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

#endif

G_END_DECLS
