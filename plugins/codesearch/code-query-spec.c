/* code-query-spec.c
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

#include "config.h"

#define _GNU_SOURCE

#include <string.h>

#include "code-query-spec-private.h"
#include "code-regex-private.h"

typedef enum _CodeQueryAstType
{
  CODE_QUERY_AST_CONTAINS = 1,
  CODE_QUERY_AST_REGEX,
} CodeQueryAstType;

typedef struct _CodeQueryAst
{
  gpointer              data;
  gsize                 datalen;
  struct _CodeQueryAst *left;
  struct _CodeQueryAst *right;
  CodeQueryAstType      type;
} CodeQueryAst;

struct _CodeQuerySpec
{
  GObject           parent_instance;
  CodePostingQuery *posting_query;
  CodeQueryAst     *tree;
};

G_DEFINE_FINAL_TYPE (CodeQuerySpec, code_query_spec, G_TYPE_OBJECT)

static CodeQueryAst *
code_query_ast_new (CodeQueryAstType type,
                    gpointer         data,
                    gsize            datalen)
{
  CodeQueryAst *ast;

  ast = g_new (CodeQueryAst, 1);
  ast->type = type;
  ast->data = data;
  ast->datalen = datalen;
  ast->left = NULL;
  ast->right = NULL;

  return ast;
}

static void
code_query_ast_free (CodeQueryAst *ast)
{
  g_clear_pointer (&ast->left, code_query_ast_free);
  g_clear_pointer (&ast->right, code_query_ast_free);

  switch (ast->type)
    {
    case CODE_QUERY_AST_CONTAINS:
      g_clear_pointer (&ast->data, g_free);
      break;

    case CODE_QUERY_AST_REGEX:
      g_clear_pointer (&ast->data, g_regex_unref);
      break;

    default:
      break;
    }

  g_free (ast);
}

static gboolean
code_query_ast_matches_contains (CodeQueryAst *ast,
                                 const guint8 *data,
                                 gsize         len)
{
  return memmem (data, len, ast->data, ast->datalen) != NULL;
}

static gboolean
code_query_ast_matches_regex (CodeQueryAst *ast,
                              const guint8 *data,
                              gsize         len)
{
  return g_regex_match_full (ast->data,
                             (const char *)data, len, 0,
                             G_REGEX_MATCH_DEFAULT,
                             NULL,
                             NULL);
}

static inline gboolean
code_query_ast_matches (CodeQueryAst *ast,
                        const char   *path,
                        GBytes       *bytes)
{
  gsize len;
  const guint8 *data = g_bytes_get_data (bytes, &len);

  if (ast->type == CODE_QUERY_AST_CONTAINS)
    return code_query_ast_matches_contains (ast, data, len);

  if (ast->type == CODE_QUERY_AST_REGEX)
    return code_query_ast_matches_regex (ast, data, len);

  return FALSE;
}

static void
code_query_spec_finalize (GObject *object)
{
  CodeQuerySpec *self = (CodeQuerySpec *)object;

  g_clear_pointer (&self->posting_query, _code_posting_query_free);
  g_clear_pointer (&self->tree, code_query_ast_free);

  G_OBJECT_CLASS (code_query_spec_parent_class)->finalize (object);
}

static void
code_query_spec_class_init (CodeQuerySpecClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = code_query_spec_finalize;
}

static void
code_query_spec_init (CodeQuerySpec *self)
{
}

CodeQuerySpec *
code_query_spec_new_contains (const char *string)
{
  CodeQuerySpec *spec;

  g_return_val_if_fail (string != NULL, NULL);

  spec = g_object_new (CODE_TYPE_QUERY_SPEC, NULL);
  spec->tree = code_query_ast_new (CODE_QUERY_AST_CONTAINS, g_strdup (string), strlen (string));
  spec->posting_query = _code_posting_query_new_for_literal (string, -1);

  return spec;
}

CodeQuerySpec *
code_query_spec_new_for_regex (GRegex *regex)
{
  CodeQuerySpec *spec;

  g_return_val_if_fail (regex != NULL, NULL);

  spec = g_object_new (CODE_TYPE_QUERY_SPEC, NULL);
  spec->tree = code_query_ast_new (CODE_QUERY_AST_REGEX, g_regex_ref (regex), 0);
  spec->posting_query = _code_regex_to_posting_query (regex);

  return spec;
}

gboolean
_code_query_spec_matches (CodeQuerySpec *spec,
                          const char    *path,
                          GBytes        *bytes)
{
  return code_query_ast_matches (spec->tree, path, bytes);
}

CodePostingQuery *
_code_query_spec_dup_posting_query (CodeQuerySpec *spec)
{
  g_return_val_if_fail (CODE_IS_QUERY_SPEC (spec), NULL);

  return _code_posting_query_copy (spec->posting_query);
}
