/* plugin-gitlab-ci-expression.c
 *
 * Copyright 2026 Christian Hergert
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

#include <string.h>

#include "plugin-gitlab-ci-error-private.h"
#include "plugin-gitlab-ci-expression-private.h"

#define PLUGIN_GITLAB_CI_EXPRESSION_MAX_LENGTH 8192
#define PLUGIN_GITLAB_CI_REGEX_MAX_LENGTH      1024
#define PLUGIN_GITLAB_CI_REGEX_MAX_INPUT       8192

typedef enum
{
  TOKEN_END,
  TOKEN_VARIABLE,
  TOKEN_STRING,
  TOKEN_REGEX,
  TOKEN_NULL,
  TOKEN_EQ,
  TOKEN_NE,
  TOKEN_MATCH,
  TOKEN_NOT_MATCH,
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_INVALID,
} TokenKind;

typedef struct
{
  TokenKind  kind;
  char      *text;
  gsize      offset;
} Token;

typedef struct
{
  const char             *input;
  const char             *cursor;
  PluginGitlabCiContext  *context;
  Token                   token;
  GError                **error;
} Parser;

typedef struct
{
  char     *string;
  gboolean  is_null;
  gboolean  is_regex;
} Value;

static void
token_clear (Token *token)
{
  g_assert (token != NULL);

  g_clear_pointer (&token->text, g_free);
  token->kind = TOKEN_END;
  token->offset = 0;
}

static void
value_clear (Value *value)
{
  g_assert (value != NULL);

  g_clear_pointer (&value->string, g_free);
}

G_GNUC_PRINTF (2, 3)
static void
set_syntax_error (Parser     *parser,
                  const char *format,
                  ...)
{
  g_autofree char *detail = NULL;
  va_list args;

  g_assert (parser != NULL);
  g_assert (format != NULL);

  if (parser->error == NULL || *parser->error != NULL)
    return;

  va_start (args, format);
  detail = g_strdup_vprintf (format, args);
  va_end (args);

  g_set_error (parser->error,
               PLUGIN_GITLAB_CI_ERROR,
               PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
               "rule expression at byte %" G_GSIZE_FORMAT ": %s",
               parser->token.offset,
               detail);
}

static char *
parse_quoted (Parser *parser,
              char    quote)
{
  g_autoptr(GString) str = NULL;

  g_assert (parser != NULL);

  str = g_string_new (NULL);

  parser->cursor++;

  while (*parser->cursor != '\0' && *parser->cursor != quote)
    {
      if (*parser->cursor == '\\' && parser->cursor[1] != '\0')
        {
          parser->cursor++;
          switch (*parser->cursor)
            {
            case 'n':
              g_string_append_c (str, '\n');
              break;

            case 'r':
              g_string_append_c (str, '\r');
              break;

            case 't':
              g_string_append_c (str, '\t');
              break;

            default:
              g_string_append_c (str, *parser->cursor);
              break;
            }
        }
      else
        {
          g_string_append_c (str, *parser->cursor);
        }

      parser->cursor++;
    }

  if (*parser->cursor != quote)
    {
      set_syntax_error (parser, "unterminated quoted string");
      return NULL;
    }

  parser->cursor++;

  return g_string_free (g_steal_pointer (&str), FALSE);
}

static char *
parse_regex (Parser *parser)
{
  g_autoptr(GString) str = NULL;
  gboolean escaped = FALSE;

  g_assert (parser != NULL);

  str = g_string_new (NULL);
  parser->cursor++;

  while (*parser->cursor != '\0')
    {
      if (!escaped && *parser->cursor == '/')
        {
          parser->cursor++;
          return g_string_free (g_steal_pointer (&str), FALSE);
        }
      if (!escaped && *parser->cursor == '\\')
        {
          escaped = TRUE;
        }
      else
        {
          escaped = FALSE;
        }

      g_string_append_c (str, *parser->cursor);
      parser->cursor++;
    }

  set_syntax_error (parser, "unterminated regular expression");
  return NULL;
}

static void
parser_next (Parser *parser)
{
  const char *start;

  g_assert (parser != NULL);

  token_clear (&parser->token);
  while (g_ascii_isspace (*parser->cursor))
    parser->cursor++;
  parser->token.offset = parser->cursor - parser->input;

  if (*parser->cursor == '\0')
    {
      parser->token.kind = TOKEN_END;
      return;
    }

  if (g_str_has_prefix (parser->cursor, "=="))
    {
      parser->token.kind = TOKEN_EQ;
      parser->cursor += 2;
      return;
    }

  if (g_str_has_prefix (parser->cursor, "!="))
    {
      parser->token.kind = TOKEN_NE;
      parser->cursor += 2;
      return;
    }

  if (g_str_has_prefix (parser->cursor, "=~"))
    {
      parser->token.kind = TOKEN_MATCH;
      parser->cursor += 2;
      return;
    }

  if (g_str_has_prefix (parser->cursor, "!~"))
    {
      parser->token.kind = TOKEN_NOT_MATCH;
      parser->cursor += 2;
      return;
    }

  if (g_str_has_prefix (parser->cursor, "&&"))
    {
      parser->token.kind = TOKEN_AND;
      parser->cursor += 2;
      return;
    }

  if (g_str_has_prefix (parser->cursor, "||"))
    {
      parser->token.kind = TOKEN_OR;
      parser->cursor += 2;
      return;
    }

  if (*parser->cursor == '(')
    {
      parser->token.kind = TOKEN_LPAREN;
      parser->cursor++;
      return;
    }

  if (*parser->cursor == ')')
    {
      parser->token.kind = TOKEN_RPAREN;
      parser->cursor++;
      return;
    }

  if (*parser->cursor == '"' || *parser->cursor == '\'')
    {
      char quote = *parser->cursor;

      parser->token.kind = TOKEN_STRING;
      parser->token.text = parse_quoted (parser, quote);
      if (parser->token.text == NULL)
        parser->token.kind = TOKEN_INVALID;
      return;
    }

  if (*parser->cursor == '/')
    {
      parser->token.kind = TOKEN_REGEX;
      parser->token.text = parse_regex (parser);

      if (parser->token.text == NULL)
        parser->token.kind = TOKEN_INVALID;

      return;
    }

  if (*parser->cursor == '$')
    {
      parser->cursor++;

      if (*parser->cursor == '{')
        {
          parser->cursor++;
          start = parser->cursor;

          while (g_ascii_isalnum (*parser->cursor) || *parser->cursor == '_')
            parser->cursor++;

          if (*parser->cursor != '}')
            {
              parser->token.kind = TOKEN_INVALID;
              set_syntax_error (parser, "unterminated variable");
              return;
            }

          parser->token.text = g_strndup (start, parser->cursor - start);
          parser->cursor++;
        }
      else
        {
          start = parser->cursor;
          while (g_ascii_isalnum (*parser->cursor) || *parser->cursor == '_')
            parser->cursor++;
          parser->token.text = g_strndup (start, parser->cursor - start);
        }

      if (parser->token.text[0] == '\0')
        {
          parser->token.kind = TOKEN_INVALID;
          set_syntax_error (parser, "invalid variable");
          return;
        }

      parser->token.kind = TOKEN_VARIABLE;

      return;
    }

  start = parser->cursor;
  while (*parser->cursor != '\0' &&
         !g_ascii_isspace (*parser->cursor) &&
         strchr ("()=!&|~", *parser->cursor) == NULL)
    parser->cursor++;

  parser->token.text = g_strndup (start, parser->cursor - start);

  if (g_str_equal (parser->token.text, "null"))
    {
      parser->token.kind = TOKEN_NULL;
    }
  else if (parser->token.text[0] != '\0')
    {
      parser->token.kind = TOKEN_STRING;
    }
  else
    {
      parser->token.kind = TOKEN_INVALID;
      set_syntax_error (parser, "unexpected character '%c'", *parser->cursor);
      parser->cursor++;
    }
}

static gboolean parse_or (Parser   *parser,
                          gboolean *result);

static gboolean
parse_value (Parser *parser,
             Value  *value)
{
  g_assert (parser != NULL);
  g_assert (value != NULL);

  switch (parser->token.kind)
    {
    case TOKEN_VARIABLE:
      {
        const char *variable;

        variable = plugin_gitlab_ci_context_get_variable (parser->context, parser->token.text);

        if (variable == NULL)
          value->is_null = TRUE;
        else
          value->string = g_strdup (variable);

        parser_next (parser);

        return TRUE;
      }

    case TOKEN_STRING:
      value->string = g_strdup (parser->token.text);
      parser_next (parser);
      return TRUE;

    case TOKEN_REGEX:
      value->string = g_strdup (parser->token.text);
      value->is_regex = TRUE;
      parser_next (parser);
      return TRUE;

    case TOKEN_NULL:
      value->is_null = TRUE;
      parser_next (parser);
      return TRUE;

    case TOKEN_END:
    case TOKEN_EQ:
    case TOKEN_NE:
    case TOKEN_MATCH:
    case TOKEN_NOT_MATCH:
    case TOKEN_AND:
    case TOKEN_OR:
    case TOKEN_LPAREN:
    case TOKEN_RPAREN:
    case TOKEN_INVALID:
    default:
      set_syntax_error (parser, "expected a value");
      return FALSE;
    }
}

static gboolean
value_truthy (Value *value)
{
  g_assert (value != NULL);

  return !value->is_null && value->string != NULL && value->string[0] != '\0';
}

static gboolean
regex_is_bounded (const char *pattern)
{
  const char *p;

  g_assert (pattern != NULL);

  if (strlen (pattern) > PLUGIN_GITLAB_CI_REGEX_MAX_LENGTH)
    return FALSE;

  for (p = pattern; p[0] != '\0' && p[1] != '\0'; p++)
    {
      if ((p[0] == '*' || p[0] == '+' || p[0] == '}') &&
          (p[1] == '*' || p[1] == '+' || p[1] == '{'))
        return FALSE;
    }

  return TRUE;
}

static gboolean
compare_values (Parser    *parser,
                Value     *left,
                TokenKind  operation,
                Value     *right,
                gboolean  *result)
{
  g_assert (parser != NULL);
  g_assert (left != NULL);
  g_assert (right != NULL);
  g_assert (result != NULL);

  switch (operation)
    {
    case TOKEN_EQ:
      *result = left->is_null == right->is_null &&
                (left->is_null || g_strcmp0 (left->string, right->string) == 0);
      return TRUE;

    case TOKEN_NE:
      *result = left->is_null != right->is_null ||
                (!left->is_null && g_strcmp0 (left->string, right->string) != 0);
      return TRUE;

    case TOKEN_MATCH:
    case TOKEN_NOT_MATCH:
      {
        g_autoptr(GRegex) regex = NULL;
        g_autoptr(GError) regex_error = NULL;
        gboolean matched;

        if (right->is_null || right->string == NULL)
          {
            set_syntax_error (parser, "regular-expression operand is null");
            return FALSE;
          }

        if (!regex_is_bounded (right->string))
          {
            set_syntax_error (parser, "regular expression exceeds safety limits");
            return FALSE;
          }

        if (left->string != NULL && strlen (left->string) > PLUGIN_GITLAB_CI_REGEX_MAX_INPUT)
          {
            set_syntax_error (parser, "regular-expression input exceeds safety limits");
            return FALSE;
          }

        if (!(regex = g_regex_new (right->string, G_REGEX_OPTIMIZE, 0, &regex_error)))
          {
            set_syntax_error (parser, "invalid regular expression: %s", regex_error->message);
            return FALSE;
          }

        matched = !left->is_null &&
                  g_regex_match (regex, left->string != NULL ? left->string : "", 0, NULL);
        *result = operation == TOKEN_MATCH ? matched : !matched;

        return TRUE;
      }

    case TOKEN_END:
    case TOKEN_VARIABLE:
    case TOKEN_STRING:
    case TOKEN_REGEX:
    case TOKEN_NULL:
    case TOKEN_AND:
    case TOKEN_OR:
    case TOKEN_LPAREN:
    case TOKEN_RPAREN:
    case TOKEN_INVALID:
    default:
      g_assert_not_reached ();
    }
}

static gboolean
parse_comparison (Parser   *parser,
                  gboolean *result)
{
  Value left = { 0 };
  Value right = { 0 };
  TokenKind operation;
  gboolean success = FALSE;

  g_assert (parser != NULL);
  g_assert (result != NULL);

  if (parser->token.kind == TOKEN_LPAREN)
    {
      parser_next (parser);

      if (!parse_or (parser, result))
        return FALSE;

      if (parser->token.kind != TOKEN_RPAREN)
        {
          set_syntax_error (parser, "expected ')'");
          return FALSE;
        }

      parser_next (parser);

      return TRUE;
    }

  if (!parse_value (parser, &left))
    goto out;

  if (parser->token.kind != TOKEN_EQ &&
      parser->token.kind != TOKEN_NE &&
      parser->token.kind != TOKEN_MATCH &&
      parser->token.kind != TOKEN_NOT_MATCH)
    {
      *result = value_truthy (&left);
      success = TRUE;
      goto out;
    }

  operation = parser->token.kind;
  parser_next (parser);
  if (!parse_value (parser, &right))
    goto out;
  success = compare_values (parser, &left, operation, &right, result);

out:
  value_clear (&left);
  value_clear (&right);
  return success;
}

static gboolean
parse_and (Parser   *parser,
           gboolean *result)
{
  gboolean left;

  g_assert (parser != NULL);
  g_assert (result != NULL);

  if (!parse_comparison (parser, &left))
    return FALSE;

  while (parser->token.kind == TOKEN_AND)
    {
      gboolean right;

      parser_next (parser);
      if (!parse_comparison (parser, &right))
        return FALSE;
      left = left && right;
    }

  *result = left;
  return TRUE;
}

static gboolean
parse_or (Parser   *parser,
          gboolean *result)
{
  gboolean left;

  g_assert (parser != NULL);
  g_assert (result != NULL);

  if (!parse_and (parser, &left))
    return FALSE;

  while (parser->token.kind == TOKEN_OR)
    {
      gboolean right;

      parser_next (parser);
      if (!parse_and (parser, &right))
        return FALSE;
      left = left || right;
    }

  *result = left;

  return TRUE;
}

gboolean
plugin_gitlab_ci_expression_evaluate (const char             *expression,
                                      PluginGitlabCiContext  *context,
                                      gboolean               *result,
                                      GError                **error)
{
  Parser parser = { 0 };
  gboolean success;

  g_return_val_if_fail (expression != NULL, FALSE);
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (strlen (expression) > PLUGIN_GITLAB_CI_EXPRESSION_MAX_LENGTH)
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_LIMIT_EXCEEDED,
                   "rule expression exceeds %u bytes",
                   PLUGIN_GITLAB_CI_EXPRESSION_MAX_LENGTH);
      return FALSE;
    }

  parser.input = expression;
  parser.cursor = expression;
  parser.context = context;
  parser.error = error;
  parser_next (&parser);
  success = parse_or (&parser, result);

  if (success && parser.token.kind != TOKEN_END)
    {
      set_syntax_error (&parser, "unexpected token");
      success = FALSE;
    }

  token_clear (&parser.token);

  return success;
}
