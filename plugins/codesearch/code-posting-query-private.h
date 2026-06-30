/* code-posting-query-private.h
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "code-array-private.h"

G_BEGIN_DECLS

typedef enum _CodePostingQueryKind
{
  CODE_POSTING_QUERY_ALL,
  CODE_POSTING_QUERY_NONE,
  CODE_POSTING_QUERY_TRIGRAM,
  CODE_POSTING_QUERY_AND,
  CODE_POSTING_QUERY_OR,
} CodePostingQueryKind;

CodePostingQuery     *_code_posting_query_new_all                      (void);
CodePostingQuery     *_code_posting_query_new_none                     (void);
CodePostingQuery     *_code_posting_query_new_trigram                  (guint               trigram_id);
CodePostingQuery     *_code_posting_query_new_for_literal              (const char         *literal,
                                                                        gssize              len);
CodePostingQuery     *_code_posting_query_and_take                     (CodePostingQuery   *left,
                                                                        CodePostingQuery   *right);
CodePostingQuery     *_code_posting_query_or_take                      (CodePostingQuery   *left,
                                                                        CodePostingQuery   *right);
CodePostingQuery     *_code_posting_query_copy                         (CodePostingQuery   *self);
void                  _code_posting_query_free                         (CodePostingQuery   *self);
CodePostingQueryKind  _code_posting_query_get_kind                     (CodePostingQuery   *self);
guint                 _code_posting_query_get_trigram_id               (CodePostingQuery   *self);
gsize                 _code_posting_query_get_n_children               (CodePostingQuery   *self);
CodePostingQuery     *_code_posting_query_get_child                    (CodePostingQuery   *self,
                                                                        gsize               position);
char                 *_code_posting_query_to_string                    (CodePostingQuery   *self);
gboolean              _code_posting_query_collect_conjunctive_trigrams (CodePostingQuery   *self,
                                                                        CodeTrigramIdArray *trigrams);
gboolean              _code_posting_query_execute                      (CodePostingQuery   *self,
                                                                        CodeIndex          *index,
                                                                        CodePostingList    *out_document_ids);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CodePostingQuery, _code_posting_query_free)

G_END_DECLS
