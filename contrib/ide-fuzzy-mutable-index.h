/* ide-fuzzy-mutable-index.h
 *
 * Copyright 2015-2025 Christian Hergert <chergert@redhat.com>
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

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _IdeFuzzyMutableIndex IdeFuzzyMutableIndex;

typedef struct
{
   const char *key;
   gpointer    value;
   float       score;
   guint       id;
} IdeFuzzyMutableIndexMatch;

IdeFuzzyMutableIndex     *ide_fuzzy_mutable_index_new                (gboolean              case_sensitive);
IdeFuzzyMutableIndex     *ide_fuzzy_mutable_index_new_with_free_func (gboolean              case_sensitive,
                                                                      GDestroyNotify        free_func);
void                      ide_fuzzy_mutable_index_set_free_func      (IdeFuzzyMutableIndex *fuzzy,
                                                                      GDestroyNotify        free_func);
void                      ide_fuzzy_mutable_index_begin_bulk_insert  (IdeFuzzyMutableIndex *fuzzy);
void                      ide_fuzzy_mutable_index_end_bulk_insert    (IdeFuzzyMutableIndex *fuzzy);
gboolean                  ide_fuzzy_mutable_index_contains           (IdeFuzzyMutableIndex *fuzzy,
                                                                      const char           *key);
void                      ide_fuzzy_mutable_index_insert             (IdeFuzzyMutableIndex *fuzzy,
                                                                      const char           *key,
                                                                      gpointer              value);
GArray                   *ide_fuzzy_mutable_index_match              (IdeFuzzyMutableIndex *fuzzy,
                                                                      const char           *needle,
                                                                      gsize                 max_matches);
void                      ide_fuzzy_mutable_index_remove             (IdeFuzzyMutableIndex *fuzzy,
                                                                      const char           *key);
IdeFuzzyMutableIndex     *ide_fuzzy_mutable_index_ref                (IdeFuzzyMutableIndex *fuzzy);
void                      ide_fuzzy_mutable_index_unref              (IdeFuzzyMutableIndex *fuzzy);
char                     *ide_fuzzy_highlight                        (const char           *str,
                                                                      const char           *query,
                                                                      gboolean              case_sensitive);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeFuzzyMutableIndex, ide_fuzzy_mutable_index_unref)

G_END_DECLS
