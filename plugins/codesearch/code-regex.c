/* code-regex.c
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

#include <stdlib.h>
#include <string.h>

#include "code-regex-private.h"

#define CODE_REGEX_MAX_EXACT 7
#define CODE_REGEX_MAX_SET 20
#define CODE_REGEX_MAX_CHAR_CLASS 100
#define CODE_REGEX_SUPPORTED_COMPILE_FLAGS \
  (G_REGEX_CASELESS | \
   G_REGEX_MULTILINE | \
   G_REGEX_DOTALL | \
   G_REGEX_ANCHORED | \
   G_REGEX_DOLLAR_ENDONLY | \
   G_REGEX_UNGREEDY | \
   G_REGEX_NO_AUTO_CAPTURE | \
   G_REGEX_OPTIMIZE | \
   G_REGEX_FIRSTLINE | \
   G_REGEX_DUPNAMES | \
   G_REGEX_NEWLINE_CR | \
   G_REGEX_NEWLINE_LF | \
   G_REGEX_NEWLINE_ANYCRLF | \
   G_REGEX_BSR_ANYCRLF)
#define CODE_REGEX_SUPPORTED_MATCH_FLAGS \
  (G_REGEX_MATCH_ANCHORED | \
   G_REGEX_MATCH_NOTBOL | \
   G_REGEX_MATCH_NOTEOL | \
   G_REGEX_MATCH_NOTEMPTY | \
   G_REGEX_MATCH_NEWLINE_CR | \
   G_REGEX_MATCH_NEWLINE_LF | \
   G_REGEX_MATCH_NEWLINE_ANY | \
   G_REGEX_MATCH_NEWLINE_ANYCRLF | \
   G_REGEX_MATCH_BSR_ANYCRLF | \
   G_REGEX_MATCH_BSR_ANY | \
   G_REGEX_MATCH_NOTEMPTY_ATSTART)

typedef enum _CodeRegexAstKind
{
  CODE_REGEX_AST_NO_MATCH,
  CODE_REGEX_AST_EMPTY,
  CODE_REGEX_AST_LITERAL,
  CODE_REGEX_AST_CHAR_CLASS,
  CODE_REGEX_AST_CONCAT,
  CODE_REGEX_AST_ALTERNATE,
  CODE_REGEX_AST_OPTIONAL,
  CODE_REGEX_AST_STAR,
  CODE_REGEX_AST_PLUS,
  CODE_REGEX_AST_REPEAT,
  CODE_REGEX_AST_CAPTURE,
  CODE_REGEX_AST_ASSERTION,
  CODE_REGEX_AST_DOT,
} CodeRegexAstKind;

typedef struct _CodeRegexAst
{
  struct _CodeRegexAst *left;
  struct _CodeRegexAst *right;
  CodeStringArray       strings;
  char                 *literal;
  guint                 min;
  guint                 max;
  CodeRegexAstKind      kind;
  guint                 fold_case : 1;
} CodeRegexAst;

typedef struct _CodeRegexParser
{
  const char *pos;
  const char *end;
  guint       fold_case : 1;
  guint       unsupported : 1;
} CodeRegexParser;

typedef struct _CodeRegexInfo
{
  CodeStringArray   exact;
  CodeStringArray   prefix;
  CodeStringArray   suffix;
  CodePostingQuery *match;
  guint             can_empty : 1;
  guint             has_exact : 1;
} CodeRegexInfo;

typedef enum _CodeRegexClassCategory
{
  CODE_REGEX_CLASS_DIGIT = 1 << 0,
  CODE_REGEX_CLASS_SPACE = 1 << 1,
  CODE_REGEX_CLASS_WORD  = 1 << 2,
} CodeRegexClassCategory;

typedef enum _CodeRegexClassItemKind
{
  CODE_REGEX_CLASS_ITEM_SINGLE,
  CODE_REGEX_CLASS_ITEM_CATEGORY,
  CODE_REGEX_CLASS_ITEM_NEGATED_CATEGORY,
  CODE_REGEX_CLASS_ITEM_TOO_LARGE,
  CODE_REGEX_CLASS_ITEM_UNSUPPORTED,
} CodeRegexClassItemKind;

typedef struct _CodeRegexClassItem
{
  CodeRegexClassItemKind kind;
  gunichar               ch;
  guint                  category;
} CodeRegexClassItem;

typedef struct _CodeRegexClassBuilder
{
  CodeStringArray strings;
  guint           positive_categories;
  guint           negative_categories;
  guint           too_large : 1;
  guint           unsupported : 1;
} CodeRegexClassBuilder;

static CodeRegexAst *code_regex_parser_parse_alternate (CodeRegexParser *parser);
static CodeRegexInfo code_regex_analyze_ast (CodeRegexAst *ast);

static char *
code_regex_string_new_for_char (gunichar ch)
{
  char buf[7];
  int len;

  len = g_unichar_to_utf8 (ch, buf);
  buf[len] = 0;

  return g_strdup (buf);
}

static gsize
code_regex_string_char_len (const char *str)
{
  g_assert (str != NULL);

  return g_utf8_strlen (str, -1);
}

static char *
code_regex_string_dup_prefix (const char *str,
                              gsize       n_chars)
{
  const char *end;

  g_assert (str != NULL);

  end = g_utf8_offset_to_pointer (str, n_chars);

  return g_strndup (str, end - str);
}

static char *
code_regex_string_dup_suffix (const char *str,
                              gsize       n_chars)
{
  gsize len;
  const char *begin;

  g_assert (str != NULL);

  len = code_regex_string_char_len (str);

  if (n_chars >= len)
    return g_strdup (str);

  begin = g_utf8_offset_to_pointer (str, len - n_chars);

  return g_strdup (begin);
}

static int
code_regex_string_compare (const void *a,
                           const void *b)
{
  const char * const *astr = a;
  const char * const *bstr = b;

  return strcmp (*astr, *bstr);
}

static int
code_regex_string_suffix_compare (const void *a,
                                  const void *b)
{
  const char * const *astr = a;
  const char * const *bstr = b;
  const char *ap;
  const char *bp;
  gsize alen;
  gsize blen;

  alen = strlen (*astr);
  blen = strlen (*bstr);
  ap = *astr + alen;
  bp = *bstr + blen;

  while (ap > *astr && bp > *bstr)
    {
      guint8 ac;
      guint8 bc;

      ac = *--ap;
      bc = *--bp;

      if (ac != bc)
        return ac < bc ? -1 : 1;
    }

  if (alen == blen)
    return 0;

  return alen < blen ? -1 : 1;
}

static void
code_regex_string_set_append_take (CodeStringArray *set,
                                   char            *str)
{
  g_assert (set != NULL);
  g_assert (str != NULL);

  code_string_array_append (set, str);
}

static void
code_regex_string_set_append (CodeStringArray *set,
                              const char      *str)
{
  g_assert (set != NULL);
  g_assert (str != NULL);

  code_regex_string_set_append_take (set, g_strdup (str));
}

static void
code_regex_string_set_clean (CodeStringArray *set,
                             gboolean         is_suffix)
{
  char **strings;
  gsize n_strings;
  gsize pos = 0;

  g_assert (set != NULL);

  strings = code_string_array_get_data (set);
  n_strings = code_string_array_get_size (set);

  if (n_strings == 0)
    return;

  qsort (strings,
         n_strings,
         sizeof (char *),
         is_suffix ? code_regex_string_suffix_compare : code_regex_string_compare);

  for (gsize i = 0; i < n_strings; i++)
    {
      if (pos > 0 && strcmp (strings[pos - 1], strings[i]) == 0)
        {
          g_free (strings[i]);
          continue;
        }

      strings[pos++] = strings[i];
    }

  set->end = set->start + pos;
}

static void
code_regex_string_set_copy (CodeStringArray       *dest,
                            const CodeStringArray *src)
{
  gsize n_strings;

  g_assert (dest != NULL);
  g_assert (src != NULL);

  n_strings = code_string_array_get_size (src);

  for (gsize i = 0; i < n_strings; i++)
    code_regex_string_set_append (dest, code_string_array_get (src, i));
}

static void
code_regex_string_set_union (CodeStringArray       *dest,
                             const CodeStringArray *src,
                             gboolean               is_suffix)
{
  g_assert (dest != NULL);
  g_assert (src != NULL);

  code_regex_string_set_copy (dest, src);
  code_regex_string_set_clean (dest, is_suffix);
}

static CodeStringArray
code_regex_string_set_cross (const CodeStringArray *left,
                             const CodeStringArray *right,
                             gboolean               is_suffix)
{
  CodeStringArray ret;
  gsize n_left;
  gsize n_right;

  g_assert (left != NULL);
  g_assert (right != NULL);

  code_string_array_init (&ret);
  n_left = code_string_array_get_size (left);
  n_right = code_string_array_get_size (right);

  for (gsize i = 0; i < n_left; i++)
    {
      const char *left_str;

      left_str = code_string_array_get (left, i);

      for (gsize j = 0; j < n_right; j++)
        {
          const char *right_str;

          right_str = code_string_array_get (right, j);
          code_regex_string_set_append_take (&ret, g_strconcat (left_str, right_str, NULL));
        }
    }

  code_regex_string_set_clean (&ret, is_suffix);

  return ret;
}

static gsize
code_regex_string_set_min_len (const CodeStringArray *set)
{
  gsize n_strings;
  gsize min_len = G_MAXSIZE;

  g_assert (set != NULL);

  n_strings = code_string_array_get_size (set);

  if (n_strings == 0)
    return 0;

  for (gsize i = 0; i < n_strings; i++)
    {
      const char *str;
      gsize len;

      str = code_string_array_get (set, i);
      len = code_regex_string_char_len (str);

      if (len < min_len)
        min_len = len;
    }

  return min_len;
}

static CodePostingQuery *
code_regex_posting_query_and_trigrams_take (CodePostingQuery *query,
                                            CodeStringArray   *strings)
{
  g_autoptr(CodePostingQuery) or_query = NULL;
  gsize n_strings;

  g_assert (query != NULL);
  g_assert (strings != NULL);

  if (code_regex_string_set_min_len (strings) < 3)
    return query;

  n_strings = code_string_array_get_size (strings);

  for (gsize i = 0; i < n_strings; i++)
    {
      CodePostingQuery *literal_query;

      literal_query = _code_posting_query_new_for_literal (code_string_array_get (strings, i),
                                                           -1);

      if (or_query == NULL)
        or_query = literal_query;
      else
        or_query = _code_posting_query_or_take (g_steal_pointer (&or_query),
                                                literal_query);
    }

  if (or_query == NULL)
    return query;

  return _code_posting_query_and_take (query, g_steal_pointer (&or_query));
}

static gboolean
code_regex_string_has_prefix (const char *str,
                              const char *prefix)
{
  return g_str_has_prefix (str, prefix);
}

static gboolean
code_regex_string_has_suffix (const char *str,
                              const char *suffix)
{
  return g_str_has_suffix (str, suffix);
}

static void
code_regex_info_init (CodeRegexInfo *info)
{
  g_assert (info != NULL);

  code_string_array_init (&info->exact);
  code_string_array_init (&info->prefix);
  code_string_array_init (&info->suffix);
  info->match = NULL;
  info->can_empty = FALSE;
  info->has_exact = FALSE;
}

static void
code_regex_info_clear (CodeRegexInfo *info)
{
  g_assert (info != NULL);

  code_string_array_clear (&info->exact);
  code_string_array_clear (&info->prefix);
  code_string_array_clear (&info->suffix);
  g_clear_pointer (&info->match, _code_posting_query_free);
  info->can_empty = FALSE;
  info->has_exact = FALSE;
}

static CodeRegexInfo
code_regex_info_any_match (void)
{
  CodeRegexInfo info;

  code_regex_info_init (&info);
  info.can_empty = TRUE;
  code_regex_string_set_append (&info.prefix, "");
  code_regex_string_set_append (&info.suffix, "");
  info.match = _code_posting_query_new_all ();

  return info;
}

static CodeRegexInfo
code_regex_info_any_char (void)
{
  CodeRegexInfo info;

  code_regex_info_init (&info);
  code_regex_string_set_append (&info.prefix, "");
  code_regex_string_set_append (&info.suffix, "");
  info.match = _code_posting_query_new_all ();

  return info;
}

static CodeRegexInfo
code_regex_info_no_match (void)
{
  CodeRegexInfo info;

  code_regex_info_init (&info);
  info.match = _code_posting_query_new_none ();

  return info;
}

static CodeRegexInfo
code_regex_info_empty_string (void)
{
  CodeRegexInfo info;

  code_regex_info_init (&info);
  info.can_empty = TRUE;
  info.has_exact = TRUE;
  code_regex_string_set_append (&info.exact, "");
  info.match = _code_posting_query_new_all ();

  return info;
}

static void
code_regex_info_add_exact (CodeRegexInfo *info)
{
  g_assert (info != NULL);

  if (info->has_exact)
    info->match = code_regex_posting_query_and_trigrams_take (info->match, &info->exact);
}

static void
code_regex_info_simplify_set (CodeRegexInfo  *info,
                              CodeStringArray *set,
                              gboolean         is_suffix)
{
  char **strings;
  gsize n_strings;
  gsize pos = 0;

  g_assert (info != NULL);
  g_assert (set != NULL);

  code_regex_string_set_clean (set, is_suffix);
  info->match = code_regex_posting_query_and_trigrams_take (info->match, set);

  for (gsize n = 3; n == 3 || code_string_array_get_size (set) > CODE_REGEX_MAX_SET; n--)
    {
      strings = code_string_array_get_data (set);
      n_strings = code_string_array_get_size (set);

      for (gsize i = 0; i < n_strings; i++)
        {
          gsize len;

          len = code_regex_string_char_len (strings[i]);

          if (len >= n)
            {
              char *shortened;

              if (!is_suffix)
                shortened = code_regex_string_dup_prefix (strings[i], n - 1);
              else
                shortened = code_regex_string_dup_suffix (strings[i], n - 1);

              g_free (strings[i]);
              strings[i] = shortened;
            }
        }

      code_regex_string_set_clean (set, is_suffix);
    }

  strings = code_string_array_get_data (set);
  n_strings = code_string_array_get_size (set);
  pos = 0;

  for (gsize i = 0; i < n_strings; i++)
    {
      gboolean redundant = FALSE;

      if (pos > 0)
        {
          if (!is_suffix)
            redundant = code_regex_string_has_prefix (strings[i], strings[pos - 1]);
          else
            redundant = code_regex_string_has_suffix (strings[i], strings[pos - 1]);
        }

      if (redundant)
        {
          g_free (strings[i]);
          continue;
        }

      strings[pos++] = strings[i];
    }

  set->end = set->start + pos;
}

static void
code_regex_info_simplify (CodeRegexInfo *info,
                          gboolean       force)
{
  g_assert (info != NULL);

  if (info->has_exact)
    {
      gsize n_exact;
      gsize min_len;

      code_regex_string_set_clean (&info->exact, FALSE);

      n_exact = code_string_array_get_size (&info->exact);
      min_len = code_regex_string_set_min_len (&info->exact);

      if (n_exact > CODE_REGEX_MAX_EXACT ||
          (min_len >= 3 && force) ||
          min_len >= 4)
        {
          code_regex_info_add_exact (info);

          for (gsize i = 0; i < n_exact; i++)
            {
              const char *str;
              gsize len;

              str = code_string_array_get (&info->exact, i);
              len = code_regex_string_char_len (str);

              if (len < 3)
                {
                  code_regex_string_set_append (&info->prefix, str);
                  code_regex_string_set_append (&info->suffix, str);
                }
              else
                {
                  code_regex_string_set_append_take (&info->prefix,
                                                     code_regex_string_dup_prefix (str, 2));
                  code_regex_string_set_append_take (&info->suffix,
                                                     code_regex_string_dup_suffix (str, 2));
                }
            }

          code_string_array_clear (&info->exact);
          code_string_array_init (&info->exact);
          info->has_exact = FALSE;
        }
    }

  if (!info->has_exact)
    {
      code_regex_info_simplify_set (info, &info->prefix, FALSE);
      code_regex_info_simplify_set (info, &info->suffix, TRUE);
    }
}

static CodeRegexInfo
code_regex_info_concat_take (CodeRegexInfo *left,
                             CodeRegexInfo *right)
{
  CodeRegexInfo ret;

  g_assert (left != NULL);
  g_assert (right != NULL);

  code_regex_info_init (&ret);
  ret.can_empty = left->can_empty && right->can_empty;
  ret.match = _code_posting_query_and_take (g_steal_pointer (&left->match),
                                            g_steal_pointer (&right->match));

  if (left->has_exact && right->has_exact)
    {
      ret.has_exact = TRUE;
      ret.exact = code_regex_string_set_cross (&left->exact, &right->exact, FALSE);
    }
  else
    {
      if (left->has_exact)
        {
          ret.prefix = code_regex_string_set_cross (&left->exact, &right->prefix, FALSE);
        }
      else
        {
          code_regex_string_set_copy (&ret.prefix, &left->prefix);

          if (left->can_empty)
            code_regex_string_set_union (&ret.prefix, &right->prefix, FALSE);
        }

      if (right->has_exact)
        {
          ret.suffix = code_regex_string_set_cross (&left->suffix, &right->exact, TRUE);
        }
      else
        {
          code_regex_string_set_copy (&ret.suffix, &right->suffix);

          if (right->can_empty)
            code_regex_string_set_union (&ret.suffix, &left->suffix, TRUE);
        }

      if (!left->has_exact &&
          !right->has_exact &&
          code_string_array_get_size (&left->suffix) <= CODE_REGEX_MAX_SET &&
          code_string_array_get_size (&right->prefix) <= CODE_REGEX_MAX_SET &&
          code_regex_string_set_min_len (&left->suffix) +
          code_regex_string_set_min_len (&right->prefix) >= 3)
        {
          CodeStringArray bridge;

          bridge = code_regex_string_set_cross (&left->suffix, &right->prefix, FALSE);
          ret.match = code_regex_posting_query_and_trigrams_take (ret.match, &bridge);
          code_string_array_clear (&bridge);
        }
    }

  code_regex_info_simplify (&ret, FALSE);
  code_regex_info_clear (left);
  code_regex_info_clear (right);

  return ret;
}

static CodeRegexInfo
code_regex_info_alternate_take (CodeRegexInfo *left,
                                CodeRegexInfo *right)
{
  CodeRegexInfo ret;

  g_assert (left != NULL);
  g_assert (right != NULL);

  code_regex_info_init (&ret);
  ret.can_empty = left->can_empty || right->can_empty;

  if (left->has_exact && right->has_exact)
    {
      ret.has_exact = TRUE;
      code_regex_string_set_copy (&ret.exact, &left->exact);
      code_regex_string_set_union (&ret.exact, &right->exact, FALSE);
    }
  else if (left->has_exact)
    {
      code_regex_string_set_copy (&ret.prefix, &left->exact);
      code_regex_string_set_union (&ret.prefix, &right->prefix, FALSE);
      code_regex_string_set_copy (&ret.suffix, &left->exact);
      code_regex_string_set_union (&ret.suffix, &right->suffix, TRUE);
      code_regex_info_add_exact (left);
    }
  else if (right->has_exact)
    {
      code_regex_string_set_copy (&ret.prefix, &left->prefix);
      code_regex_string_set_union (&ret.prefix, &right->exact, FALSE);
      code_regex_string_set_copy (&ret.suffix, &left->suffix);
      code_regex_string_set_union (&ret.suffix, &right->exact, TRUE);
      code_regex_info_add_exact (right);
    }
  else
    {
      code_regex_string_set_copy (&ret.prefix, &left->prefix);
      code_regex_string_set_union (&ret.prefix, &right->prefix, FALSE);
      code_regex_string_set_copy (&ret.suffix, &left->suffix);
      code_regex_string_set_union (&ret.suffix, &right->suffix, TRUE);
    }

  ret.match = _code_posting_query_or_take (g_steal_pointer (&left->match),
                                           g_steal_pointer (&right->match));

  code_regex_info_simplify (&ret, FALSE);
  code_regex_info_clear (left);
  code_regex_info_clear (right);

  return ret;
}

static CodeRegexAst *
code_regex_ast_new (CodeRegexAstKind kind)
{
  CodeRegexAst *ast;

  ast = g_new0 (CodeRegexAst, 1);
  ast->kind = kind;
  code_string_array_init (&ast->strings);

  return ast;
}

static CodeRegexAst *
code_regex_ast_new_literal_take (char     *literal,
                                 gboolean  fold_case)
{
  CodeRegexAst *ast;

  g_assert (literal != NULL);

  ast = code_regex_ast_new (CODE_REGEX_AST_LITERAL);
  ast->literal = literal;
  ast->fold_case = fold_case;

  return ast;
}

static CodeRegexAst *
code_regex_ast_new_binary_take (CodeRegexAstKind  kind,
                                CodeRegexAst     *left,
                                CodeRegexAst     *right)
{
  CodeRegexAst *ast;

  g_assert (kind == CODE_REGEX_AST_CONCAT ||
            kind == CODE_REGEX_AST_ALTERNATE);
  g_assert (left != NULL);
  g_assert (right != NULL);

  ast = code_regex_ast_new (kind);
  ast->left = left;
  ast->right = right;

  return ast;
}

static CodeRegexAst *
code_regex_ast_new_unary_take (CodeRegexAstKind  kind,
                               CodeRegexAst     *child)
{
  CodeRegexAst *ast;

  g_assert (child != NULL);

  ast = code_regex_ast_new (kind);
  ast->left = child;

  return ast;
}

static void
code_regex_ast_free (CodeRegexAst *ast)
{
  if (ast == NULL)
    return;

  g_clear_pointer (&ast->left, code_regex_ast_free);
  g_clear_pointer (&ast->right, code_regex_ast_free);
  code_string_array_clear (&ast->strings);
  g_free (ast->literal);
  g_free (ast);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CodeRegexAst, code_regex_ast_free)

static gboolean
code_regex_parser_peek_byte (CodeRegexParser *parser,
                             char             ch)
{
  g_assert (parser != NULL);

  return parser->pos < parser->end && parser->pos[0] == ch;
}

static gboolean
code_regex_parser_get_char (CodeRegexParser  *parser,
                            gunichar         *ch,
                            const char      **begin,
                            const char      **end)
{
  gunichar current;
  const char *next;

  g_assert (parser != NULL);
  g_assert (ch != NULL);

  if (parser->pos >= parser->end)
    return FALSE;

  current = g_utf8_get_char_validated (parser->pos, parser->end - parser->pos);

  if (current == (gunichar)-1 || current == (gunichar)-2)
    {
      parser->unsupported = TRUE;
      return FALSE;
    }

  next = g_utf8_next_char (parser->pos);

  if (begin != NULL)
    *begin = parser->pos;

  if (end != NULL)
    *end = next;

  parser->pos = next;
  *ch = current;

  return TRUE;
}

static gboolean
code_regex_parser_consume_byte (CodeRegexParser *parser,
                                char             ch)
{
  g_assert (parser != NULL);

  if (!code_regex_parser_peek_byte (parser, ch))
    return FALSE;

  parser->pos++;

  return TRUE;
}

static gboolean
code_regex_parser_parse_uint (CodeRegexParser *parser,
                              guint           *value)
{
  guint ret = 0;
  gboolean found = FALSE;

  g_assert (parser != NULL);
  g_assert (value != NULL);

  while (parser->pos < parser->end && g_ascii_isdigit (parser->pos[0]))
    {
      guint digit = parser->pos[0] - '0';

      found = TRUE;

      if (ret > (G_MAXUINT - digit) / 10)
        {
          parser->unsupported = TRUE;
          return FALSE;
        }

      ret = ret * 10 + digit;
      parser->pos++;
    }

  *value = ret;

  return found;
}

static void
code_regex_class_builder_init (CodeRegexClassBuilder *builder)
{
  g_assert (builder != NULL);

  code_string_array_init (&builder->strings);
  builder->positive_categories = 0;
  builder->negative_categories = 0;
  builder->too_large = FALSE;
  builder->unsupported = FALSE;
}

static void
code_regex_class_builder_clear (CodeRegexClassBuilder *builder)
{
  g_assert (builder != NULL);

  code_string_array_clear (&builder->strings);
}

static void
code_regex_class_builder_add_char (CodeRegexClassBuilder *builder,
                                   gunichar               ch,
                                   gboolean               fold_case)
{
  g_assert (builder != NULL);

  if (code_string_array_get_size (&builder->strings) > CODE_REGEX_MAX_CHAR_CLASS)
    {
      builder->too_large = TRUE;
      return;
    }

  if (fold_case && g_unichar_isalpha (ch))
    {
      gunichar lower;
      gunichar upper;

      lower = g_unichar_tolower (ch);
      upper = g_unichar_toupper (ch);

      if (lower > 0x7F || upper > 0x7F)
        {
          builder->too_large = TRUE;
          return;
        }

      code_regex_string_set_append_take (&builder->strings,
                                         code_regex_string_new_for_char (lower));

      if (upper != lower)
        code_regex_string_set_append_take (&builder->strings,
                                           code_regex_string_new_for_char (upper));

      return;
    }

  code_regex_string_set_append_take (&builder->strings,
                                     code_regex_string_new_for_char (ch));
}

static void
code_regex_class_builder_add_range (CodeRegexClassBuilder *builder,
                                    gunichar               first,
                                    gunichar               last,
                                    gboolean               fold_case)
{
  guint count;

  g_assert (builder != NULL);

  if (first > last)
    {
      builder->unsupported = TRUE;
      return;
    }

  count = last - first + 1;

  if (count > CODE_REGEX_MAX_CHAR_CLASS)
    {
      builder->too_large = TRUE;
      return;
    }

  for (gunichar ch = first; ch <= last; ch++)
    code_regex_class_builder_add_char (builder, ch, fold_case);
}

static void
code_regex_class_builder_add_category (CodeRegexClassBuilder *builder,
                                       guint                  category,
                                       gboolean               negated,
                                       gboolean               fold_case)
{
  g_assert (builder != NULL);

  if (negated)
    builder->negative_categories |= category;
  else
    builder->positive_categories |= category;

  if ((builder->positive_categories & builder->negative_categories) != 0)
    return;

  if (negated)
    {
      builder->too_large = TRUE;
      return;
    }

  if (category == CODE_REGEX_CLASS_DIGIT)
    {
      code_regex_class_builder_add_range (builder, '0', '9', FALSE);
      return;
    }

  if (category == CODE_REGEX_CLASS_WORD)
    {
      code_regex_class_builder_add_range (builder, '0', '9', FALSE);
      code_regex_class_builder_add_range (builder, 'A', 'Z', fold_case);
      code_regex_class_builder_add_char (builder, '_', FALSE);
      code_regex_class_builder_add_range (builder, 'a', 'z', fold_case);
      return;
    }

  if (category == CODE_REGEX_CLASS_SPACE)
    {
      code_regex_class_builder_add_char (builder, ' ', FALSE);
      code_regex_class_builder_add_char (builder, '\t', FALSE);
      code_regex_class_builder_add_char (builder, '\n', FALSE);
      code_regex_class_builder_add_char (builder, '\r', FALSE);
      code_regex_class_builder_add_char (builder, '\f', FALSE);
      code_regex_class_builder_add_char (builder, '\v', FALSE);
      return;
    }

  g_assert_not_reached ();
}

static gboolean
code_regex_class_builder_covers_all (CodeRegexClassBuilder *builder)
{
  g_assert (builder != NULL);

  return (builder->positive_categories & builder->negative_categories) != 0;
}

static CodeRegexClassItem
code_regex_class_item_new_single (gunichar ch)
{
  return (CodeRegexClassItem) {
    .kind = CODE_REGEX_CLASS_ITEM_SINGLE,
    .ch = ch,
  };
}

static CodeRegexClassItem
code_regex_class_item_new_category (guint    category,
                                    gboolean negated)
{
  return (CodeRegexClassItem) {
    .kind = negated ? CODE_REGEX_CLASS_ITEM_NEGATED_CATEGORY : CODE_REGEX_CLASS_ITEM_CATEGORY,
    .category = category,
  };
}

static CodeRegexClassItem
code_regex_class_item_new_too_large (void)
{
  return (CodeRegexClassItem) {
    .kind = CODE_REGEX_CLASS_ITEM_TOO_LARGE,
  };
}

static CodeRegexClassItem
code_regex_class_item_new_unsupported (void)
{
  return (CodeRegexClassItem) {
    .kind = CODE_REGEX_CLASS_ITEM_UNSUPPORTED,
  };
}

static CodeRegexClassItem
code_regex_parser_parse_class_escape (CodeRegexParser *parser)
{
  gunichar ch;

  g_assert (parser != NULL);

  if (!code_regex_parser_get_char (parser, &ch, NULL, NULL))
    return code_regex_class_item_new_unsupported ();

  switch (ch)
    {
    case 'd':
      return code_regex_class_item_new_category (CODE_REGEX_CLASS_DIGIT, FALSE);

    case 'D':
      return code_regex_class_item_new_category (CODE_REGEX_CLASS_DIGIT, TRUE);

    case 's':
      return code_regex_class_item_new_category (CODE_REGEX_CLASS_SPACE, FALSE);

    case 'S':
      return code_regex_class_item_new_category (CODE_REGEX_CLASS_SPACE, TRUE);

    case 'w':
      return code_regex_class_item_new_category (CODE_REGEX_CLASS_WORD, FALSE);

    case 'W':
      return code_regex_class_item_new_category (CODE_REGEX_CLASS_WORD, TRUE);

    case 'b':
      return code_regex_class_item_new_single ('\b');

    case 'n':
      return code_regex_class_item_new_single ('\n');

    case 'r':
      return code_regex_class_item_new_single ('\r');

    case 't':
      return code_regex_class_item_new_single ('\t');

    case 'f':
      return code_regex_class_item_new_single ('\f');

    case 'v':
    case 'h':
    case 'H':
    case 'V':
      return code_regex_class_item_new_too_large ();

    case 'p':
    case 'P':
    case 'C':
    case 'N':
    case 'R':
    case 'X':
    case 'c':
    case 'g':
    case 'k':
    case 'o':
    case 'x':
    case 'u':
    case 'U':
      return code_regex_class_item_new_too_large ();

    default:
      if (g_unichar_isdigit (ch))
        return code_regex_class_item_new_unsupported ();

      return code_regex_class_item_new_single (ch);
    }
}

static CodeRegexClassItem
code_regex_parser_parse_class_item (CodeRegexParser *parser)
{
  gunichar ch;

  g_assert (parser != NULL);

  if (!code_regex_parser_get_char (parser, &ch, NULL, NULL))
    return code_regex_class_item_new_unsupported ();

  if (ch == '\\')
    return code_regex_parser_parse_class_escape (parser);

  return code_regex_class_item_new_single (ch);
}

static void
code_regex_class_builder_add_item (CodeRegexClassBuilder *builder,
                                   CodeRegexClassItem    *item,
                                   gboolean               fold_case)
{
  g_assert (builder != NULL);
  g_assert (item != NULL);

  switch (item->kind)
    {
    case CODE_REGEX_CLASS_ITEM_SINGLE:
      code_regex_class_builder_add_char (builder, item->ch, fold_case);
      return;

    case CODE_REGEX_CLASS_ITEM_CATEGORY:
      code_regex_class_builder_add_category (builder, item->category, FALSE, fold_case);
      return;

    case CODE_REGEX_CLASS_ITEM_NEGATED_CATEGORY:
      code_regex_class_builder_add_category (builder, item->category, TRUE, fold_case);
      return;

    case CODE_REGEX_CLASS_ITEM_TOO_LARGE:
      builder->too_large = TRUE;
      return;

    case CODE_REGEX_CLASS_ITEM_UNSUPPORTED:
      builder->unsupported = TRUE;
      return;

    default:
      g_assert_not_reached ();
    }
}

static gboolean
code_regex_parser_parse_range_end (CodeRegexParser     *parser,
                                   CodeRegexClassItem  *end_item)
{
  g_assert (parser != NULL);
  g_assert (end_item != NULL);

  if (parser->pos >= parser->end || code_regex_parser_peek_byte (parser, ']'))
    return FALSE;

  *end_item = code_regex_parser_parse_class_item (parser);

  return end_item->kind == CODE_REGEX_CLASS_ITEM_SINGLE;
}

static CodeRegexAst *
code_regex_parser_parse_char_class (CodeRegexParser *parser)
{
  CodeRegexClassBuilder builder;
  CodeRegexAst *ast;
  gboolean negated = FALSE;
  gboolean first = TRUE;

  g_assert (parser != NULL);

  code_regex_class_builder_init (&builder);

  if (code_regex_parser_consume_byte (parser, '^'))
    negated = TRUE;

  while (parser->pos < parser->end)
    {
      CodeRegexClassItem item;

      if (!first && code_regex_parser_consume_byte (parser, ']'))
        break;

      item = code_regex_parser_parse_class_item (parser);

      if (item.kind == CODE_REGEX_CLASS_ITEM_SINGLE &&
          parser->pos < parser->end &&
          parser->pos[0] == '-' &&
          parser->pos + 1 < parser->end &&
          parser->pos[1] != ']')
        {
          CodeRegexClassItem end_item;

          parser->pos++;

          if (!code_regex_parser_parse_range_end (parser, &end_item))
            {
              builder.unsupported = TRUE;
              break;
            }

          code_regex_class_builder_add_range (&builder,
                                              item.ch,
                                              end_item.ch,
                                              parser->fold_case);
        }
      else
        {
          code_regex_class_builder_add_item (&builder, &item, parser->fold_case);
        }

      first = FALSE;
    }

  if (parser->pos >= parser->end && (first || parser->end[-1] != ']'))
    builder.unsupported = TRUE;

  if (builder.unsupported)
    {
      code_regex_class_builder_clear (&builder);
      parser->unsupported = TRUE;
      return code_regex_ast_new (CODE_REGEX_AST_DOT);
    }

  if (code_regex_class_builder_covers_all (&builder))
    {
      code_regex_class_builder_clear (&builder);

      if (negated)
        return code_regex_ast_new (CODE_REGEX_AST_NO_MATCH);

      return code_regex_ast_new (CODE_REGEX_AST_DOT);
    }

  if (negated || builder.too_large)
    {
      code_regex_class_builder_clear (&builder);
      return code_regex_ast_new (CODE_REGEX_AST_DOT);
    }

  code_regex_string_set_clean (&builder.strings, FALSE);

  if (code_string_array_get_size (&builder.strings) == 0)
    {
      code_regex_class_builder_clear (&builder);
      return code_regex_ast_new (CODE_REGEX_AST_NO_MATCH);
    }

  ast = code_regex_ast_new (CODE_REGEX_AST_CHAR_CLASS);
  ast->strings = builder.strings;
  code_string_array_init (&builder.strings);
  code_regex_class_builder_clear (&builder);

  return ast;
}

static CodeRegexAst *
code_regex_parser_literal_for_char (CodeRegexParser *parser,
                                    gunichar         ch)
{
  g_assert (parser != NULL);

  return code_regex_ast_new_literal_take (code_regex_string_new_for_char (ch),
                                          parser->fold_case);
}

static CodeRegexAst *
code_regex_parser_parse_escape (CodeRegexParser *parser)
{
  gunichar ch;

  g_assert (parser != NULL);

  if (!code_regex_parser_get_char (parser, &ch, NULL, NULL))
    {
      parser->unsupported = TRUE;
      return code_regex_ast_new (CODE_REGEX_AST_DOT);
    }

  switch (ch)
    {
    case 'A':
    case 'G':
    case 'Z':
    case 'z':
    case 'b':
    case 'B':
      return code_regex_ast_new (CODE_REGEX_AST_ASSERTION);

    case 'd':
    case 'D':
    case 's':
    case 'S':
    case 'w':
    case 'W':
    case 'p':
    case 'P':
      return code_regex_ast_new (CODE_REGEX_AST_DOT);

    case 'n':
      return code_regex_parser_literal_for_char (parser, '\n');

    case 'r':
      return code_regex_parser_literal_for_char (parser, '\r');

    case 't':
      return code_regex_parser_literal_for_char (parser, '\t');

    case 'f':
      return code_regex_parser_literal_for_char (parser, '\f');

    case 'h':
    case 'H':
    case 'N':
    case 'v':
    case 'V':
      return code_regex_ast_new (CODE_REGEX_AST_DOT);

    case 'C':
    case 'K':
    case 'R':
    case 'X':
    case 'c':
    case 'g':
    case 'k':
    case 'o':
    case 'Q':
    case 'x':
    case 'u':
    case 'U':
      parser->unsupported = TRUE;
      return code_regex_ast_new (CODE_REGEX_AST_DOT);

    default:
      if (g_unichar_isdigit (ch))
        {
          parser->unsupported = TRUE;
          return code_regex_ast_new (CODE_REGEX_AST_DOT);
        }

      return code_regex_parser_literal_for_char (parser, ch);
    }
}

static gboolean
code_regex_parser_parse_options (CodeRegexParser *parser,
                                 gboolean        *scoped,
                                 gboolean        *fold_case)
{
  gboolean negative = FALSE;

  g_assert (parser != NULL);
  g_assert (scoped != NULL);
  g_assert (fold_case != NULL);

  *scoped = FALSE;
  *fold_case = parser->fold_case;

  while (parser->pos < parser->end)
    {
      char ch = parser->pos[0];

      if (ch == ':')
        {
          parser->pos++;
          *scoped = TRUE;
          return TRUE;
        }

      if (ch == ')')
        {
          parser->pos++;
          return TRUE;
        }

      parser->pos++;

      if (ch == '-')
        {
          negative = TRUE;
          continue;
        }

      if (ch == 'i')
        {
          *fold_case = !negative;
          continue;
        }

      if (ch == 's' || ch == 'm' || ch == 'U')
        continue;

      return FALSE;
    }

  return FALSE;
}

static CodeRegexAst *
code_regex_parser_parse_group (CodeRegexParser *parser)
{
  g_autoptr(CodeRegexAst) child = NULL;
  gboolean old_fold_case;

  g_assert (parser != NULL);

  old_fold_case = parser->fold_case;

  if (code_regex_parser_consume_byte (parser, '?'))
    {
      gboolean scoped;
      gboolean fold_case;

      if (code_regex_parser_consume_byte (parser, ':'))
        {
          child = code_regex_parser_parse_alternate (parser);
          parser->fold_case = old_fold_case;

          if (!code_regex_parser_consume_byte (parser, ')'))
            parser->unsupported = TRUE;

          return g_steal_pointer (&child);
        }

      if (!code_regex_parser_parse_options (parser, &scoped, &fold_case))
        {
          parser->unsupported = TRUE;
          return code_regex_ast_new (CODE_REGEX_AST_EMPTY);
        }

      if (scoped)
        {
          parser->fold_case = fold_case;
          child = code_regex_parser_parse_alternate (parser);
          parser->fold_case = old_fold_case;

          if (!code_regex_parser_consume_byte (parser, ')'))
            parser->unsupported = TRUE;

          return g_steal_pointer (&child);
        }

      parser->fold_case = fold_case;

      return code_regex_ast_new (CODE_REGEX_AST_EMPTY);
    }

  child = code_regex_parser_parse_alternate (parser);
  parser->fold_case = old_fold_case;

  if (!code_regex_parser_consume_byte (parser, ')'))
    parser->unsupported = TRUE;

  return code_regex_ast_new_unary_take (CODE_REGEX_AST_CAPTURE,
                                        g_steal_pointer (&child));
}

static gboolean
code_regex_char_is_atom_end (gunichar ch)
{
  return ch == '|' || ch == ')' || ch == 0;
}

static CodeRegexAst *
code_regex_parser_parse_atom (CodeRegexParser *parser)
{
  gunichar ch;

  g_assert (parser != NULL);

  if (parser->pos >= parser->end)
    return NULL;

  if (code_regex_char_is_atom_end (parser->pos[0]))
    return NULL;

  if (!code_regex_parser_get_char (parser, &ch, NULL, NULL))
    return NULL;

  switch (ch)
    {
    case '.':
      return code_regex_ast_new (CODE_REGEX_AST_DOT);

    case '^':
    case '$':
      return code_regex_ast_new (CODE_REGEX_AST_ASSERTION);

    case '(':
      return code_regex_parser_parse_group (parser);

    case '[':
      return code_regex_parser_parse_char_class (parser);

    case '\\':
      return code_regex_parser_parse_escape (parser);

    case '*':
    case '+':
    case '?':
    case '{':
      parser->unsupported = TRUE;
      return code_regex_ast_new (CODE_REGEX_AST_DOT);

    default:
      return code_regex_parser_literal_for_char (parser, ch);
    }
}

static CodeRegexAst *
code_regex_parser_parse_repeat (CodeRegexParser *parser,
                                CodeRegexAst    *atom)
{
  g_autoptr(CodeRegexAst) owned_atom = atom;

  g_assert (parser != NULL);
  g_assert (owned_atom != NULL);

  if (code_regex_parser_consume_byte (parser, '?'))
    return code_regex_ast_new_unary_take (CODE_REGEX_AST_OPTIONAL,
                                          g_steal_pointer (&owned_atom));

  if (code_regex_parser_consume_byte (parser, '*'))
    return code_regex_ast_new_unary_take (CODE_REGEX_AST_STAR,
                                          g_steal_pointer (&owned_atom));

  if (code_regex_parser_consume_byte (parser, '+'))
    return code_regex_ast_new_unary_take (CODE_REGEX_AST_PLUS,
                                          g_steal_pointer (&owned_atom));

  if (code_regex_parser_consume_byte (parser, '{'))
    {
      CodeRegexAst *repeat;
      guint min = 0;
      guint max = G_MAXUINT;

      if (!code_regex_parser_parse_uint (parser, &min))
        {
          parser->unsupported = TRUE;
          return g_steal_pointer (&owned_atom);
        }

      if (code_regex_parser_consume_byte (parser, ','))
        {
          if (parser->pos < parser->end && g_ascii_isdigit (parser->pos[0]))
            {
              if (!code_regex_parser_parse_uint (parser, &max))
                {
                  parser->unsupported = TRUE;
                  return g_steal_pointer (&owned_atom);
                }
            }
        }
      else
        {
          max = min;
        }

      if (!code_regex_parser_consume_byte (parser, '}') || min > max)
        {
          parser->unsupported = TRUE;
          return g_steal_pointer (&owned_atom);
        }

      repeat = code_regex_ast_new_unary_take (CODE_REGEX_AST_REPEAT,
                                              g_steal_pointer (&owned_atom));
      repeat->min = min;
      repeat->max = max;

      return repeat;
    }

  return g_steal_pointer (&owned_atom);
}

static CodeRegexAst *
code_regex_parser_parse_piece (CodeRegexParser *parser)
{
  g_autoptr(CodeRegexAst) atom = NULL;
  CodeRegexAst *ret;

  g_assert (parser != NULL);

  if (!(atom = code_regex_parser_parse_atom (parser)))
    return NULL;

  ret = code_regex_parser_parse_repeat (parser, g_steal_pointer (&atom));

  if (parser->pos < parser->end &&
      (parser->pos[0] == '?' || parser->pos[0] == '+'))
    parser->pos++;

  return ret;
}

static CodeRegexAst *
code_regex_parser_parse_concat (CodeRegexParser *parser)
{
  g_autoptr(CodeRegexAst) ret = NULL;

  g_assert (parser != NULL);

  while (parser->pos < parser->end &&
         parser->pos[0] != ')' &&
         parser->pos[0] != '|')
    {
      g_autoptr(CodeRegexAst) piece = NULL;

      if (!(piece = code_regex_parser_parse_piece (parser)))
        break;

      if (ret == NULL)
        ret = g_steal_pointer (&piece);
      else
        ret = code_regex_ast_new_binary_take (CODE_REGEX_AST_CONCAT,
                                              g_steal_pointer (&ret),
                                              g_steal_pointer (&piece));
    }

  if (ret == NULL)
    return code_regex_ast_new (CODE_REGEX_AST_EMPTY);

  return g_steal_pointer (&ret);
}

static CodeRegexAst *
code_regex_parser_parse_alternate (CodeRegexParser *parser)
{
  g_autoptr(CodeRegexAst) ret = NULL;

  g_assert (parser != NULL);

  ret = code_regex_parser_parse_concat (parser);

  while (code_regex_parser_consume_byte (parser, '|'))
    {
      g_autoptr(CodeRegexAst) right = NULL;

      right = code_regex_parser_parse_concat (parser);
      ret = code_regex_ast_new_binary_take (CODE_REGEX_AST_ALTERNATE,
                                            g_steal_pointer (&ret),
                                            g_steal_pointer (&right));
    }

  return g_steal_pointer (&ret);
}

static CodeRegexInfo
code_regex_analyze_literal (CodeRegexAst *ast)
{
  CodeRegexInfo info;
  gunichar ch;

  g_assert (ast != NULL);
  g_assert (ast->kind == CODE_REGEX_AST_LITERAL);

  if (!ast->fold_case)
    {
      info = code_regex_info_empty_string ();
      code_string_array_clear (&info.exact);
      code_string_array_init (&info.exact);
      code_regex_string_set_append (&info.exact, ast->literal);

      return info;
    }

  ch = g_utf8_get_char (ast->literal);

  if (!g_unichar_isalpha (ch))
    {
      info = code_regex_info_empty_string ();
      code_string_array_clear (&info.exact);
      code_string_array_init (&info.exact);
      code_regex_string_set_append (&info.exact, ast->literal);

      return info;
    }

  if (g_unichar_tolower (ch) > 0x7F || g_unichar_toupper (ch) > 0x7F)
    return code_regex_info_any_char ();

  info = code_regex_info_empty_string ();
  code_string_array_clear (&info.exact);
  code_string_array_init (&info.exact);
  code_regex_string_set_append_take (&info.exact,
                                     code_regex_string_new_for_char (g_unichar_tolower (ch)));
  code_regex_string_set_append_take (&info.exact,
                                     code_regex_string_new_for_char (g_unichar_toupper (ch)));
  code_regex_string_set_clean (&info.exact, FALSE);

  return info;
}

static CodeRegexInfo
code_regex_analyze_char_class (CodeRegexAst *ast)
{
  CodeRegexInfo info;

  g_assert (ast != NULL);
  g_assert (ast->kind == CODE_REGEX_AST_CHAR_CLASS);

  info = code_regex_info_empty_string ();
  code_string_array_clear (&info.exact);
  code_string_array_init (&info.exact);
  code_regex_string_set_copy (&info.exact, &ast->strings);

  return info;
}

static CodeRegexInfo
code_regex_analyze_repeating_take (CodeRegexInfo *info)
{
  g_assert (info != NULL);

  if (info->has_exact)
    {
      code_regex_string_set_copy (&info->prefix, &info->exact);
      code_regex_string_set_copy (&info->suffix, &info->exact);
      code_string_array_clear (&info->exact);
      code_string_array_init (&info->exact);
      info->has_exact = FALSE;
    }

  return *info;
}

static CodeRegexInfo
code_regex_analyze_ast (CodeRegexAst *ast)
{
  CodeRegexInfo left;
  CodeRegexInfo right;

  g_assert (ast != NULL);

  switch (ast->kind)
    {
    case CODE_REGEX_AST_NO_MATCH:
      return code_regex_info_no_match ();

    case CODE_REGEX_AST_EMPTY:
    case CODE_REGEX_AST_ASSERTION:
      return code_regex_info_empty_string ();

    case CODE_REGEX_AST_LITERAL:
      left = code_regex_analyze_literal (ast);
      code_regex_info_simplify (&left, FALSE);
      return left;

    case CODE_REGEX_AST_CHAR_CLASS:
      left = code_regex_analyze_char_class (ast);
      code_regex_info_simplify (&left, FALSE);
      return left;

    case CODE_REGEX_AST_DOT:
      return code_regex_info_any_char ();

    case CODE_REGEX_AST_CAPTURE:
      return code_regex_analyze_ast (ast->left);

    case CODE_REGEX_AST_CONCAT:
      left = code_regex_analyze_ast (ast->left);
      right = code_regex_analyze_ast (ast->right);
      return code_regex_info_concat_take (&left, &right);

    case CODE_REGEX_AST_ALTERNATE:
      left = code_regex_analyze_ast (ast->left);
      right = code_regex_analyze_ast (ast->right);
      return code_regex_info_alternate_take (&left, &right);

    case CODE_REGEX_AST_OPTIONAL:
      left = code_regex_analyze_ast (ast->left);
      right = code_regex_info_empty_string ();
      return code_regex_info_alternate_take (&left, &right);

    case CODE_REGEX_AST_STAR:
      return code_regex_info_any_match ();

    case CODE_REGEX_AST_PLUS:
      left = code_regex_analyze_ast (ast->left);
      return code_regex_analyze_repeating_take (&left);

    case CODE_REGEX_AST_REPEAT:
      if (ast->min == 0)
        return code_regex_info_any_match ();

      left = code_regex_analyze_ast (ast->left);
      return code_regex_analyze_repeating_take (&left);

    default:
      g_return_val_if_reached (code_regex_info_any_match ());
    }
}

static CodePostingQuery *
code_regex_pattern_to_posting_query_full (const char         *pattern,
                                          GRegexCompileFlags  compile_flags)
{
  CodeRegexParser parser;
  g_autoptr(CodeRegexAst) ast = NULL;
  CodePostingQuery *ret;
  CodeRegexInfo info;

  g_return_val_if_fail (pattern != NULL, NULL);

  parser = (CodeRegexParser) {
    .pos = pattern,
    .end = pattern + strlen (pattern),
    .fold_case = !!(compile_flags & G_REGEX_CASELESS),
  };

  ast = code_regex_parser_parse_alternate (&parser);

  if (parser.unsupported || parser.pos != parser.end)
    return _code_posting_query_new_all ();

  info = code_regex_analyze_ast (ast);
  code_regex_info_simplify (&info, TRUE);
  code_regex_info_add_exact (&info);
  ret = g_steal_pointer (&info.match);
  code_regex_info_clear (&info);

  if (ret == NULL)
    ret = _code_posting_query_new_all ();

  return ret;
}

CodePostingQuery *
_code_regex_pattern_to_posting_query (const char *pattern)
{
  return code_regex_pattern_to_posting_query_full (pattern, G_REGEX_DEFAULT);
}

CodePostingQuery *
_code_regex_to_posting_query (GRegex *regex)
{
  GRegexCompileFlags compile_flags;
  GRegexMatchFlags match_flags;

  g_return_val_if_fail (regex != NULL, NULL);

  compile_flags = g_regex_get_compile_flags (regex);
  match_flags = g_regex_get_match_flags (regex);

  if ((compile_flags & ~CODE_REGEX_SUPPORTED_COMPILE_FLAGS) != 0 ||
      (match_flags & ~CODE_REGEX_SUPPORTED_MATCH_FLAGS) != 0)
    return _code_posting_query_new_all ();

  return code_regex_pattern_to_posting_query_full (g_regex_get_pattern (regex), compile_flags);
}
