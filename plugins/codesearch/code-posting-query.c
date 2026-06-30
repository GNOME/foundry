/* code-posting-query.c
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

#include "code-index-private.h"
#include "code-posting-query-private.h"

struct _CodePostingQuery
{
  CodePostingQueryArray children;
  guint                 trigram_id;
  CodePostingQueryKind  kind;
};

static CodePostingQuery *
code_posting_query_simplify_take (CodePostingQuery *self);

static CodePostingQuery *
code_posting_query_new (CodePostingQueryKind kind)
{
  CodePostingQuery *self;

  self = g_new0 (CodePostingQuery, 1);
  self->kind = kind;
  code_posting_query_array_init (&self->children);

  return self;
}

CodePostingQuery *
_code_posting_query_new_all (void)
{
  return code_posting_query_new (CODE_POSTING_QUERY_ALL);
}

CodePostingQuery *
_code_posting_query_new_none (void)
{
  return code_posting_query_new (CODE_POSTING_QUERY_NONE);
}

CodePostingQuery *
_code_posting_query_new_trigram (guint trigram_id)
{
  CodePostingQuery *self;

  self = code_posting_query_new (CODE_POSTING_QUERY_TRIGRAM);
  self->trigram_id = trigram_id;

  return self;
}

void
_code_posting_query_free (CodePostingQuery *self)
{
  gsize n_children;

  if (self == NULL)
    return;

  n_children = code_posting_query_array_get_size (&self->children);

  for (gsize i = 0; i < n_children; i++)
    _code_posting_query_free (code_posting_query_array_get (&self->children, i));

  code_posting_query_array_clear (&self->children);
  g_free (self);
}

static void
code_posting_query_append_child (CodePostingQuery *self,
                                 CodePostingQuery *child)
{
  g_assert (self != NULL);
  g_assert (child != NULL);

  code_posting_query_array_append (&self->children, child);
}

static gboolean
code_posting_query_equal (CodePostingQuery *left,
                          CodePostingQuery *right)
{
  gsize left_n_children;
  gsize right_n_children;

  g_assert (left != NULL);
  g_assert (right != NULL);

  if (left == right)
    return TRUE;

  if (left->kind != right->kind)
    return FALSE;

  if (left->kind == CODE_POSTING_QUERY_TRIGRAM)
    return left->trigram_id == right->trigram_id;

  left_n_children = code_posting_query_array_get_size (&left->children);
  right_n_children = code_posting_query_array_get_size (&right->children);

  if (left_n_children != right_n_children)
    return FALSE;

  for (gsize i = 0; i < left_n_children; i++)
    {
      CodePostingQuery *left_child;
      CodePostingQuery *right_child;

      left_child = code_posting_query_array_get (&left->children, i);
      right_child = code_posting_query_array_get (&right->children, i);

      if (!code_posting_query_equal (left_child, right_child))
        return FALSE;
    }

  return TRUE;
}

static gboolean
code_posting_query_implies (CodePostingQuery *left,
                            CodePostingQuery *right)
{
  gsize n_children;

  g_assert (left != NULL);
  g_assert (right != NULL);

  if (left->kind == CODE_POSTING_QUERY_NONE ||
      right->kind == CODE_POSTING_QUERY_ALL)
    return TRUE;

  if (left->kind == CODE_POSTING_QUERY_ALL ||
      right->kind == CODE_POSTING_QUERY_NONE)
    return FALSE;

  if (code_posting_query_equal (left, right))
    return TRUE;

  if (right->kind == CODE_POSTING_QUERY_AND)
    {
      n_children = code_posting_query_array_get_size (&right->children);

      for (gsize i = 0; i < n_children; i++)
        {
          CodePostingQuery *child;

          child = code_posting_query_array_get (&right->children, i);

          if (!code_posting_query_implies (left, child))
            return FALSE;
        }

      return TRUE;
    }

  if (left->kind == CODE_POSTING_QUERY_OR)
    {
      n_children = code_posting_query_array_get_size (&left->children);

      for (gsize i = 0; i < n_children; i++)
        {
          CodePostingQuery *child;

          child = code_posting_query_array_get (&left->children, i);

          if (!code_posting_query_implies (child, right))
            return FALSE;
        }

      return TRUE;
    }

  if (right->kind == CODE_POSTING_QUERY_OR)
    {
      n_children = code_posting_query_array_get_size (&right->children);

      for (gsize i = 0; i < n_children; i++)
        {
          CodePostingQuery *child;

          child = code_posting_query_array_get (&right->children, i);

          if (code_posting_query_implies (left, child))
            return TRUE;
        }

      return FALSE;
    }

  if (left->kind == CODE_POSTING_QUERY_AND)
    {
      n_children = code_posting_query_array_get_size (&left->children);

      for (gsize i = 0; i < n_children; i++)
        {
          CodePostingQuery *child;

          child = code_posting_query_array_get (&left->children, i);

          if (code_posting_query_implies (child, right))
            return TRUE;
        }

      return FALSE;
    }

  if (left->kind == CODE_POSTING_QUERY_TRIGRAM &&
      right->kind == CODE_POSTING_QUERY_TRIGRAM)
    return left->trigram_id == right->trigram_id;

  return FALSE;
}

static gboolean
code_trigram_id_array_contains (CodeTrigramIdArray *trigram_ids,
                                guint               trigram_id)
{
  gsize n_trigram_ids;

  g_assert (trigram_ids != NULL);

  n_trigram_ids = code_trigram_id_array_get_size (trigram_ids);

  for (gsize i = 0; i < n_trigram_ids; i++)
    {
      if (code_trigram_id_array_get (trigram_ids, i) == trigram_id)
        return TRUE;
    }

  return FALSE;
}

static void
code_posting_query_collect_common_direct_trigrams (CodePostingQuery *left,
                                                   CodePostingQuery *right,
                                                   CodeTrigramIdArray *common)
{
  gsize left_n_children;
  gsize right_n_children;

  g_assert (left != NULL);
  g_assert (right != NULL);
  g_assert (common != NULL);

  left_n_children = code_posting_query_array_get_size (&left->children);
  right_n_children = code_posting_query_array_get_size (&right->children);

  for (gsize i = 0; i < left_n_children; i++)
    {
      CodePostingQuery *left_child;

      left_child = code_posting_query_array_get (&left->children, i);

      if (left_child->kind != CODE_POSTING_QUERY_TRIGRAM)
        continue;

      for (gsize j = 0; j < right_n_children; j++)
        {
          CodePostingQuery *right_child;

          right_child = code_posting_query_array_get (&right->children, j);

          if (right_child->kind == CODE_POSTING_QUERY_TRIGRAM &&
              right_child->trigram_id == left_child->trigram_id &&
              !code_trigram_id_array_contains (common, left_child->trigram_id))
            code_trigram_id_array_append (common, left_child->trigram_id);
        }
    }
}

static void
code_posting_query_remove_direct_trigrams (CodePostingQuery *self,
                                           CodeTrigramIdArray *trigram_ids)
{
  CodePostingQuery **children;
  gsize n_children;
  gsize pos = 0;

  g_assert (self != NULL);
  g_assert (trigram_ids != NULL);

  children = code_posting_query_array_get_data (&self->children);
  n_children = code_posting_query_array_get_size (&self->children);

  if (n_children == 0)
    return;

  for (gsize i = 0; i < n_children; i++)
    {
      CodePostingQuery *child = children[i];

      if (child->kind == CODE_POSTING_QUERY_TRIGRAM &&
          code_trigram_id_array_contains (trigram_ids, child->trigram_id))
        {
          _code_posting_query_free (child);
          continue;
        }

      children[pos++] = child;
    }

  self->children.end = self->children.start + pos;
}

static CodePostingQuery *
code_posting_query_new_for_trigram_ids (CodePostingQueryKind  kind,
                                        CodeTrigramIdArray   *trigram_ids)
{
  CodePostingQuery *self;
  gsize n_trigram_ids;

  g_assert (kind == CODE_POSTING_QUERY_AND ||
            kind == CODE_POSTING_QUERY_OR);
  g_assert (trigram_ids != NULL);

  self = code_posting_query_new (kind);
  n_trigram_ids = code_trigram_id_array_get_size (trigram_ids);

  for (gsize i = 0; i < n_trigram_ids; i++)
    code_posting_query_append_child (self,
                                     _code_posting_query_new_trigram (
                                       code_trigram_id_array_get (trigram_ids, i)));

  return code_posting_query_simplify_take (self);
}

static void
code_posting_query_deduplicate_direct_trigrams (CodePostingQuery *self)
{
  CodePostingQuery **children;
  gsize n_children;
  gsize pos = 0;

  g_assert (self != NULL);
  g_assert (self->kind == CODE_POSTING_QUERY_AND ||
            self->kind == CODE_POSTING_QUERY_OR);

  children = code_posting_query_array_get_data (&self->children);
  n_children = code_posting_query_array_get_size (&self->children);

  if (n_children == 0)
    return;

  for (gsize i = 0; i < n_children; i++)
    {
      CodePostingQuery *child = children[i];
      gboolean duplicate = FALSE;

      if (child->kind == CODE_POSTING_QUERY_TRIGRAM)
        {
          for (gsize j = 0; j < pos; j++)
            {
              if (children[j]->kind == CODE_POSTING_QUERY_TRIGRAM &&
                  children[j]->trigram_id == child->trigram_id)
                {
                  duplicate = TRUE;
                  break;
                }
            }
        }

      if (duplicate)
        _code_posting_query_free (child);
      else
        children[pos++] = child;
    }

  self->children.end = self->children.start + pos;
}

static CodePostingQuery *
code_posting_query_simplify_take (CodePostingQuery *self)
{
  g_autofree CodePostingQuery **children = NULL;
  gsize n_children;

  g_assert (self != NULL);

  if (self->kind != CODE_POSTING_QUERY_AND &&
      self->kind != CODE_POSTING_QUERY_OR)
    return self;

  n_children = code_posting_query_array_get_size (&self->children);
  children = code_posting_query_array_steal (&self->children);

  for (gsize i = 0; i < n_children; i++)
    {
      CodePostingQuery *child = code_posting_query_simplify_take (children[i]);

      if (self->kind == CODE_POSTING_QUERY_AND &&
          child->kind == CODE_POSTING_QUERY_NONE)
        {
          _code_posting_query_free (child);

          for (gsize j = i + 1; j < n_children; j++)
            _code_posting_query_free (children[j]);

          _code_posting_query_free (self);
          return _code_posting_query_new_none ();
        }

      if (self->kind == CODE_POSTING_QUERY_OR &&
          child->kind == CODE_POSTING_QUERY_ALL)
        {
          _code_posting_query_free (child);

          for (gsize j = i + 1; j < n_children; j++)
            _code_posting_query_free (children[j]);

          _code_posting_query_free (self);
          return _code_posting_query_new_all ();
        }

      if ((self->kind == CODE_POSTING_QUERY_AND &&
           child->kind == CODE_POSTING_QUERY_ALL) ||
          (self->kind == CODE_POSTING_QUERY_OR &&
           child->kind == CODE_POSTING_QUERY_NONE))
        {
          _code_posting_query_free (child);
          continue;
        }

      if (child->kind == self->kind)
        {
          g_autofree CodePostingQuery **grandchildren = NULL;
          gsize n_grandchildren;

          n_grandchildren = code_posting_query_array_get_size (&child->children);
          grandchildren = code_posting_query_array_steal (&child->children);

          for (gsize j = 0; j < n_grandchildren; j++)
            code_posting_query_append_child (self, grandchildren[j]);

          g_free (child);
          continue;
        }

      code_posting_query_append_child (self, child);
    }

  code_posting_query_deduplicate_direct_trigrams (self);

  n_children = code_posting_query_array_get_size (&self->children);

  if (n_children == 0)
    {
      CodePostingQueryKind kind = self->kind;

      _code_posting_query_free (self);

      if (kind == CODE_POSTING_QUERY_AND)
        return _code_posting_query_new_all ();

      return _code_posting_query_new_none ();
    }

  if (n_children == 1)
    {
      CodePostingQuery *child = code_posting_query_array_get (&self->children, 0);

      code_posting_query_array_set_size (&self->children, 0);
      _code_posting_query_free (self);

      return child;
    }

  return self;
}

static CodePostingQuery *
code_posting_query_and_or_take (CodePostingQuery     *left,
                                CodePostingQuery     *right,
                                CodePostingQueryKind  kind)
{
  CodePostingQuery *self;
  CodePostingQueryKind other_kind;

  g_assert (left != NULL);
  g_assert (right != NULL);
  g_assert (kind == CODE_POSTING_QUERY_AND ||
            kind == CODE_POSTING_QUERY_OR);

  left = code_posting_query_simplify_take (left);
  right = code_posting_query_simplify_take (right);

  if (code_posting_query_implies (left, right))
    {
      if (kind == CODE_POSTING_QUERY_AND)
        {
          _code_posting_query_free (right);
          return left;
        }

      _code_posting_query_free (left);
      return right;
    }

  if (code_posting_query_implies (right, left))
    {
      if (kind == CODE_POSTING_QUERY_AND)
        {
          _code_posting_query_free (left);
          return right;
        }

      _code_posting_query_free (right);
      return left;
    }

  other_kind = kind == CODE_POSTING_QUERY_AND
             ? CODE_POSTING_QUERY_OR
             : CODE_POSTING_QUERY_AND;

  if (left->kind == other_kind && right->kind == other_kind)
    {
      g_auto(CodeTrigramIdArray) common;

      code_trigram_id_array_init (&common);
      code_posting_query_collect_common_direct_trigrams (left, right, &common);

      if (code_trigram_id_array_get_size (&common) > 0)
        {
          CodePostingQuery *common_query;
          CodePostingQuery *factored_query;
          CodePostingQuery *remaining_query;

          code_posting_query_remove_direct_trigrams (left, &common);
          code_posting_query_remove_direct_trigrams (right, &common);

          remaining_query = code_posting_query_and_or_take (left, right, kind);
          common_query = code_posting_query_new_for_trigram_ids (other_kind, &common);
          factored_query = code_posting_query_and_or_take (common_query,
                                                           remaining_query,
                                                           other_kind);

          return factored_query;
        }
    }

  self = code_posting_query_new (kind);
  code_posting_query_append_child (self, left);
  code_posting_query_append_child (self, right);

  return code_posting_query_simplify_take (self);
}

CodePostingQuery *
_code_posting_query_and_take (CodePostingQuery *left,
                              CodePostingQuery *right)
{
  g_return_val_if_fail (left != NULL, NULL);
  g_return_val_if_fail (right != NULL, NULL);

  return code_posting_query_and_or_take (left, right, CODE_POSTING_QUERY_AND);
}

CodePostingQuery *
_code_posting_query_or_take (CodePostingQuery *left,
                             CodePostingQuery *right)
{
  g_return_val_if_fail (left != NULL, NULL);
  g_return_val_if_fail (right != NULL, NULL);

  return code_posting_query_and_or_take (left, right, CODE_POSTING_QUERY_OR);
}

CodePostingQuery *
_code_posting_query_new_for_literal (const char *literal,
                                     gssize      len)
{
  CodePostingQuery *self = NULL;
  CodeTrigramIter iter;
  CodeTrigram trigram;

  g_return_val_if_fail (literal != NULL, NULL);

  code_trigram_iter_init (&iter, literal, len);

  while (code_trigram_iter_next (&iter, &trigram))
    {
      CodePostingQuery *child;
      guint trigram_id;

      trigram_id = code_trigram_encode (&trigram);
      child = _code_posting_query_new_trigram (trigram_id);

      if (self == NULL)
        self = child;
      else
        self = _code_posting_query_and_take (self, child);
    }

  if (self == NULL)
    return _code_posting_query_new_all ();

  return self;
}

CodePostingQuery *
_code_posting_query_copy (CodePostingQuery *self)
{
  CodePostingQuery *copy;
  gsize n_children;

  g_return_val_if_fail (self != NULL, NULL);

  copy = code_posting_query_new (self->kind);
  copy->trigram_id = self->trigram_id;

  n_children = code_posting_query_array_get_size (&self->children);

  for (gsize i = 0; i < n_children; i++)
    {
      CodePostingQuery *child;

      child = code_posting_query_array_get (&self->children, i);
      code_posting_query_append_child (copy, _code_posting_query_copy (child));
    }

  return copy;
}

CodePostingQueryKind
_code_posting_query_get_kind (CodePostingQuery *self)
{
  g_return_val_if_fail (self != NULL, CODE_POSTING_QUERY_NONE);

  return self->kind;
}

guint
_code_posting_query_get_trigram_id (CodePostingQuery *self)
{
  g_return_val_if_fail (self != NULL, 0);
  g_return_val_if_fail (self->kind == CODE_POSTING_QUERY_TRIGRAM, 0);

  return self->trigram_id;
}

gsize
_code_posting_query_get_n_children (CodePostingQuery *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return code_posting_query_array_get_size (&self->children);
}

CodePostingQuery *
_code_posting_query_get_child (CodePostingQuery *self,
                               gsize             position)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (position < code_posting_query_array_get_size (&self->children),
                        NULL);

  return code_posting_query_array_get (&self->children, position);
}

static void
code_posting_query_append_quoted_trigram (GString *string,
                                          guint    trigram_id)
{
  CodeTrigram trigram;
  gunichar chars[3];

  g_assert (string != NULL);

  trigram = code_trigram_decode (trigram_id);
  chars[0] = trigram.x;
  chars[1] = trigram.y;
  chars[2] = trigram.z;

  g_string_append_c (string, '"');

  for (gsize i = 0; i < G_N_ELEMENTS (chars); i++)
    {
      guint8 ch = chars[i] & 0xFF;

      if (ch == '"' || ch == '\\')
        {
          g_string_append_c (string, '\\');
          g_string_append_c (string, ch);
        }
      else if (ch == '\n')
        {
          g_string_append (string, "\\n");
        }
      else if (ch == '\r')
        {
          g_string_append (string, "\\r");
        }
      else if (ch == '\t')
        {
          g_string_append (string, "\\t");
        }
      else if (g_ascii_isprint (ch))
        {
          g_string_append_c (string, ch);
        }
      else
        {
          g_string_append_printf (string, "\\x%02x", ch);
        }
    }

  g_string_append_c (string, '"');
}

static gboolean
code_posting_query_needs_or_child_parens (CodePostingQuery *self)
{
  g_assert (self != NULL);

  return self->kind == CODE_POSTING_QUERY_AND &&
         code_posting_query_array_get_size (&self->children) > 1;
}

static void
code_posting_query_append_to_string (CodePostingQuery *self,
                                     GString          *string)
{
  gsize n_children;

  g_assert (self != NULL);
  g_assert (string != NULL);

  switch (self->kind)
    {
    case CODE_POSTING_QUERY_ALL:
      g_string_append_c (string, '+');
      return;

    case CODE_POSTING_QUERY_NONE:
      g_string_append_c (string, '-');
      return;

    case CODE_POSTING_QUERY_TRIGRAM:
      code_posting_query_append_quoted_trigram (string, self->trigram_id);
      return;

    case CODE_POSTING_QUERY_AND:
      n_children = code_posting_query_array_get_size (&self->children);

      if (n_children == 0)
        {
          g_string_append_c (string, '+');
          return;
        }

      for (gsize i = 0; i < n_children; i++)
        {
          if (i > 0)
            g_string_append_c (string, ' ');

          code_posting_query_append_to_string (
            code_posting_query_array_get (&self->children, i),
            string);
        }

      return;

    case CODE_POSTING_QUERY_OR:
      n_children = code_posting_query_array_get_size (&self->children);

      if (n_children == 0)
        {
          g_string_append_c (string, '-');
          return;
        }

      g_string_append_c (string, '(');

      for (gsize i = 0; i < n_children; i++)
        {
          CodePostingQuery *child;
          gboolean needs_parens;

          child = code_posting_query_array_get (&self->children, i);
          needs_parens = code_posting_query_needs_or_child_parens (child);

          if (i > 0)
            g_string_append_c (string, '|');

          if (needs_parens)
            g_string_append_c (string, '(');

          code_posting_query_append_to_string (child, string);

          if (needs_parens)
            g_string_append_c (string, ')');
        }

      g_string_append_c (string, ')');
      return;

    default:
      g_return_if_reached ();
    }
}

char *
_code_posting_query_to_string (CodePostingQuery *self)
{
  GString *string;

  g_return_val_if_fail (self != NULL, NULL);

  string = g_string_new (NULL);
  code_posting_query_append_to_string (self, string);

  return g_string_free (string, FALSE);
}

gboolean
_code_posting_query_collect_conjunctive_trigrams (CodePostingQuery *self,
                                                  CodeTrigramIdArray *trigrams)
{
  gsize old_size;
  gsize n_children;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (trigrams != NULL, FALSE);

  switch (self->kind)
    {
    case CODE_POSTING_QUERY_ALL:
      return TRUE;

    case CODE_POSTING_QUERY_NONE:
      return FALSE;

    case CODE_POSTING_QUERY_TRIGRAM:
      code_trigram_id_array_append (trigrams, self->trigram_id);
      return TRUE;

    case CODE_POSTING_QUERY_AND:
      old_size = code_trigram_id_array_get_size (trigrams);
      n_children = code_posting_query_array_get_size (&self->children);

      for (gsize i = 0; i < n_children; i++)
        {
          if (!_code_posting_query_collect_conjunctive_trigrams (
                code_posting_query_array_get (&self->children, i),
                trigrams))
            {
              code_trigram_id_array_set_size (trigrams, old_size);
              return FALSE;
            }
        }

      return TRUE;

    case CODE_POSTING_QUERY_OR:
      return FALSE;

    default:
      g_return_val_if_reached (FALSE);
    }
}

static gboolean
code_posting_list_is_sorted (CodePostingList *self)
{
  const guint *ids;
  gsize n_ids;

  g_assert (self != NULL);

  ids = code_posting_list_get_data (self);
  n_ids = code_posting_list_get_size (self);

  for (gsize i = 1; i < n_ids; i++)
    {
      if (ids[i - 1] >= ids[i])
        return FALSE;
    }

  return TRUE;
}

static void
code_posting_list_move (CodePostingList *dest,
                        CodePostingList *src)
{
  g_assert (dest != NULL);
  g_assert (src != NULL);

  code_posting_list_clear (dest);
  *dest = *src;
  code_posting_list_init (src);
}

static void
code_posting_list_append_list (CodePostingList *self,
                               CodePostingList *other)
{
  const guint *ids;
  gsize old_size;
  gsize n_ids;

  g_assert (self != NULL);
  g_assert (other != NULL);
  g_assert (code_posting_list_is_sorted (self));
  g_assert (code_posting_list_is_sorted (other));

  old_size = code_posting_list_get_size (self);
  n_ids = code_posting_list_get_size (other);

  if (n_ids == 0)
    return;

  if G_UNLIKELY (n_ids > G_MAXSIZE / sizeof (guint) - old_size)
    g_error ("requesting array size of %zu, but maximum size is %zu",
             n_ids, G_MAXSIZE / sizeof (guint) - old_size);

  ids = code_posting_list_get_data (other);

  code_posting_list_reserve (self, old_size + n_ids);
  memcpy (self->end, ids, n_ids * sizeof (guint));
  self->end += n_ids;

  g_assert (code_posting_list_is_sorted (self));
}

static void
code_posting_list_intersect (CodePostingList *self,
                             CodePostingList *other)
{
  guint *self_ids;
  const guint *other_ids;
  gsize n_self_ids;
  gsize n_other_ids;
  gsize i = 0;
  gsize j = 0;
  gsize pos = 0;

  g_assert (self != NULL);
  g_assert (other != NULL);
  g_assert (code_posting_list_is_sorted (self));
  g_assert (code_posting_list_is_sorted (other));

  if (self == other)
    return;

  self_ids = code_posting_list_get_data (self);
  other_ids = code_posting_list_get_data (other);
  n_self_ids = code_posting_list_get_size (self);
  n_other_ids = code_posting_list_get_size (other);

  while (i < n_self_ids && j < n_other_ids)
    {
      if (self_ids[i] == other_ids[j])
        {
          self_ids[pos++] = self_ids[i];
          i++;
          j++;
        }
      else if (self_ids[i] < other_ids[j])
        {
          i++;
        }
      else
        {
          j++;
        }
    }

  code_posting_list_set_size (self, pos);

  g_assert (code_posting_list_is_sorted (self));
}

static void
code_posting_list_union (CodePostingList *self,
                         CodePostingList *other)
{
  g_autofree guint *self_ids = NULL;
  const guint *other_ids;
  gsize n_self_ids;
  gsize n_other_ids;
  gsize i = 0;
  gsize j = 0;

  g_assert (self != NULL);
  g_assert (other != NULL);
  g_assert (code_posting_list_is_sorted (self));
  g_assert (code_posting_list_is_sorted (other));

  if (other == self || code_posting_list_get_size (other) == 0)
    return;

  n_self_ids = code_posting_list_get_size (self);
  n_other_ids = code_posting_list_get_size (other);

  if (n_self_ids == 0)
    {
      code_posting_list_append_list (self, other);
      return;
    }

  if G_UNLIKELY (n_other_ids > G_MAXSIZE / sizeof (guint) - n_self_ids)
    g_error ("requesting array size of %zu, but maximum size is %zu",
             n_other_ids, G_MAXSIZE / sizeof (guint) - n_self_ids);

  self_ids = code_posting_list_steal (self);
  other_ids = code_posting_list_get_data (other);

  code_posting_list_reserve (self, n_self_ids + n_other_ids);

  while (i < n_self_ids && j < n_other_ids)
    {
      if (self_ids[i] == other_ids[j])
        {
          code_posting_list_append (self, self_ids[i]);
          i++;
          j++;
        }
      else if (self_ids[i] < other_ids[j])
        {
          code_posting_list_append (self, self_ids[i++]);
        }
      else
        {
          code_posting_list_append (self, other_ids[j++]);
        }
    }

  while (i < n_self_ids)
    code_posting_list_append (self, self_ids[i++]);

  while (j < n_other_ids)
    code_posting_list_append (self, other_ids[j++]);

  g_assert (code_posting_list_is_sorted (self));
}

static void
code_posting_query_execute_restricted (CodePostingQuery *self,
                                       CodeIndex        *index,
                                       CodePostingList  *restrict_ids,
                                       CodePostingList  *out_document_ids);

static void
code_posting_query_execute_trigram_restricted (CodePostingQuery *self,
                                               CodeIndex        *index,
                                               CodePostingList  *restrict_ids,
                                               CodePostingList  *out_document_ids)
{
  CodeIndexIter iter;
  CodeTrigram trigram;
  const guint *ids;
  gsize n_ids;

  g_assert (self != NULL);
  g_assert (self->kind == CODE_POSTING_QUERY_TRIGRAM);
  g_assert (index != NULL);
  g_assert (restrict_ids != NULL);
  g_assert (out_document_ids != NULL);
  g_assert (code_posting_list_is_sorted (restrict_ids));

  n_ids = code_posting_list_get_size (restrict_ids);

  if (n_ids == 0)
    return;

  trigram = code_trigram_decode (self->trigram_id);

  if (!code_index_iter_init (&iter, index, &trigram))
    return;

  ids = code_posting_list_get_data (restrict_ids);

  for (gsize i = 0; i < n_ids; i++)
    {
      if (code_index_iter_seek_to (&iter, ids[i]))
        code_posting_list_append (out_document_ids, ids[i]);
    }
}

static void
code_posting_query_execute_and (CodePostingQuery *self,
                                CodeIndex        *index,
                                CodePostingList  *restrict_ids,
                                CodePostingList  *out_document_ids)
{
  CodePostingList current;
  CodePostingList next;
  gsize n_children;

  g_assert (self != NULL);
  g_assert (self->kind == CODE_POSTING_QUERY_AND);
  g_assert (index != NULL);
  g_assert (out_document_ids != NULL);

  if (restrict_ids != NULL && code_posting_list_get_size (restrict_ids) == 0)
    return;

  n_children = code_posting_query_array_get_size (&self->children);

  if (n_children == 0)
    {
      if (restrict_ids != NULL)
        code_posting_list_append_list (out_document_ids, restrict_ids);
      else
        _code_index_get_all_document_ids (index, out_document_ids);

      return;
    }

  code_posting_list_init (&current);
  code_posting_query_execute_restricted (code_posting_query_array_get (&self->children, 0),
                                         index,
                                         restrict_ids,
                                         &current);

  for (gsize i = 1; i < n_children && code_posting_list_get_size (&current) > 0; i++)
    {
      code_posting_list_init (&next);
      code_posting_query_execute_restricted (code_posting_query_array_get (&self->children, i),
                                             index,
                                             &current,
                                             &next);
      code_posting_list_clear (&current);
      current = next;
    }

  code_posting_list_move (out_document_ids, &current);
  code_posting_list_clear (&current);
}

static void
code_posting_query_execute_or (CodePostingQuery *self,
                               CodeIndex        *index,
                               CodePostingList  *restrict_ids,
                               CodePostingList  *out_document_ids)
{
  gsize n_children;

  g_assert (self != NULL);
  g_assert (self->kind == CODE_POSTING_QUERY_OR);
  g_assert (index != NULL);
  g_assert (out_document_ids != NULL);

  if (restrict_ids != NULL && code_posting_list_get_size (restrict_ids) == 0)
    return;

  n_children = code_posting_query_array_get_size (&self->children);

  for (gsize i = 0; i < n_children; i++)
    {
      CodePostingList child_ids;

      code_posting_list_init (&child_ids);
      code_posting_query_execute_restricted (code_posting_query_array_get (&self->children, i),
                                             index,
                                             restrict_ids,
                                             &child_ids);
      code_posting_list_union (out_document_ids, &child_ids);
      code_posting_list_clear (&child_ids);
    }

  if (restrict_ids != NULL)
    code_posting_list_intersect (out_document_ids, restrict_ids);
}

static void
code_posting_query_execute_restricted (CodePostingQuery *self,
                                       CodeIndex        *index,
                                       CodePostingList  *restrict_ids,
                                       CodePostingList  *out_document_ids)
{
  g_assert (self != NULL);
  g_assert (index != NULL);
  g_assert (out_document_ids != NULL);
  g_assert (restrict_ids == NULL || code_posting_list_is_sorted (restrict_ids));

  switch (self->kind)
    {
    case CODE_POSTING_QUERY_ALL:
      if (restrict_ids != NULL)
        code_posting_list_append_list (out_document_ids, restrict_ids);
      else
        _code_index_get_all_document_ids (index, out_document_ids);
      return;

    case CODE_POSTING_QUERY_NONE:
      return;

    case CODE_POSTING_QUERY_TRIGRAM:
      if (restrict_ids != NULL)
        {
          code_posting_query_execute_trigram_restricted (self,
                                                         index,
                                                         restrict_ids,
                                                         out_document_ids);
          return;
        }

      _code_index_get_posting_list (index, self->trigram_id, out_document_ids);
      return;

    case CODE_POSTING_QUERY_AND:
      code_posting_query_execute_and (self, index, restrict_ids, out_document_ids);
      return;

    case CODE_POSTING_QUERY_OR:
      code_posting_query_execute_or (self, index, restrict_ids, out_document_ids);
      return;

    default:
      g_return_if_reached ();
    }
}

gboolean
_code_posting_query_execute (CodePostingQuery *self,
                             CodeIndex        *index,
                             CodePostingList  *out_document_ids)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (index != NULL, FALSE);
  g_return_val_if_fail (out_document_ids != NULL, FALSE);

  code_posting_list_set_size (out_document_ids, 0);
  code_posting_query_execute_restricted (self, index, NULL, out_document_ids);

  g_return_val_if_fail (code_posting_list_is_sorted (out_document_ids), FALSE);

  return TRUE;
}
