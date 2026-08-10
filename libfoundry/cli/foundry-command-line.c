/* foundry-command-line.c
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

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include <json-glib/json-glib.h>

#include "foundry-cli-command-private.h"
#include "foundry-cli-command-tree-private.h"
#include "foundry-cli-command-tree.h"
#include "foundry-command-line-private.h"
#include "foundry-command-line-input-private.h"
#include "foundry-command-line-local-private.h"
#include "foundry-command-line-remote-private.h"
#include "foundry-ipc.h"
#include "foundry-init-private.h"
#include "foundry-tty-auth-provider.h"
#include "foundry-util-private.h"

/**
 * FoundryCommandLine:
 *
 * Abstract base class for command line interface implementations.
 *
 * FoundryCommandLine provides the core interface for command line operations including
 * command parsing, execution, and communication with remote services. Concrete
 * implementations handle different command line modes and provide unified access
 * to Foundry functionality from the terminal.
 */

G_DEFINE_ABSTRACT_TYPE (FoundryCommandLine, foundry_command_line, G_TYPE_OBJECT)
G_DEFINE_QUARK (foundry_command_line_error, foundry_command_line_error)
G_DEFINE_ENUM_TYPE (FoundryObjectSerializerFormat, foundry_object_serializer_format,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_OBJECT_SERIALIZER_FORMAT_TEXT, "text"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_OBJECT_SERIALIZER_FORMAT_JSON, "json"))

static DexCancellable *
foundry_command_line_dup_cancellable (FoundryCommandLine *self)
{
  g_assert (FOUNDRY_IS_COMMAND_LINE (self));

  if (FOUNDRY_COMMAND_LINE_GET_CLASS (self)->dup_cancellable)
    return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->dup_cancellable (self);

  return dex_cancellable_new ();
}

static void
remove_arg (char  **argv,
            guint   position)
{
  guint argc;

  g_assert (argv != NULL);
  g_assert (argv[position] != NULL);

  argc = g_strv_length (argv);
  g_free (argv[position]);
  memmove (&argv[position],
           &argv[position + 1],
           sizeof (char *) * (argc - position));
}

static gboolean
take_help_request (char **argv)
{
  gboolean help = FALSE;

  g_assert (argv != NULL);
  g_assert (argv[0] != NULL);

  if (g_strcmp0 (argv[1], "help") == 0)
    {
      remove_arg (argv, 1);
      help = TRUE;
    }

  for (guint i = 1; argv[i] != NULL;)
    {
      if (g_str_equal (argv[i], "--"))
        break;

      if (g_str_equal (argv[i], "-h") || g_str_equal (argv[i], "--help"))
        {
          remove_arg (argv, i);
          help = TRUE;
          continue;
        }

      i++;
    }

  return help;
}

static DexFuture *
foundry_command_line_real_run (FoundryCommandLine *self,
                               const char * const *argv)
{
  g_autoptr(FoundryCliOptions) options = NULL;
  g_autoptr(DexCancellable) cancellable = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *help = NULL;
  const FoundryCliCommand *command;
  FoundryCliCommandTree *tree;
  g_auto(GStrv) original_args = NULL;
  g_auto(GStrv) args = NULL;

  g_assert (FOUNDRY_IS_COMMAND_LINE (self));
  g_assert (argv != NULL);

  if (argv[0] == NULL)
    return dex_future_new_for_int (EXIT_FAILURE);

  args = g_strdupv ((char **)argv);

  /* Be sure our plugins have loaded before producing tree-based help. */
  _foundry_init_plugins ();

  tree = foundry_cli_command_tree_get_default ();

  if (g_strcmp0 (args[1], "--version") == 0)
    {
      foundry_command_line_print (self, "foundry %s\n", PACKAGE_VERSION);
      return dex_future_new_for_int (EXIT_SUCCESS);
    }

  if (take_help_request (args))
    {
      if ((help = _foundry_cli_command_tree_get_help (tree,
                                                      (const char * const *)args,
                                                      &error)))
        {
          foundry_command_line_print (self, "%s", help);
          return dex_future_new_for_int (EXIT_SUCCESS);
        }

      foundry_command_line_printerr (self, "%s: %s\n", _("error"), error->message);
      return dex_future_new_for_int (EXIT_FAILURE);
    }

  if (args[1] == NULL)
    {
      foundry_command_line_help (self);
      return dex_future_new_for_int (EXIT_FAILURE);
    }

  original_args = g_strdupv (args);

  if (!(command = foundry_cli_command_tree_lookup (tree, &args, &options, &error)))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        {
          g_autoptr(GError) help_error = NULL;

          if ((help = _foundry_cli_command_tree_get_help (tree,
                                                          (const char * const *)original_args,
                                                          &help_error)))
            {
              foundry_command_line_printerr (self, "%s", help);
              return dex_future_new_for_int (EXIT_FAILURE);
            }

          if (help_error != NULL)
            {
              g_clear_error (&error);
              g_set_error_literal (&error,
                                   help_error->domain,
                                   help_error->code,
                                   help_error->message);
            }
        }

      foundry_command_line_printerr (self, "%s: %s\n", _("error"), error->message);
      return dex_future_new_for_int (EXIT_FAILURE);
    }

  if (command->run == NULL)
    {
      g_clear_error (&error);

      if ((help = _foundry_cli_command_tree_get_help (tree,
                                                      (const char * const *)original_args,
                                                      &error)))
        foundry_command_line_printerr (self, "%s", help);
      else
        foundry_command_line_printerr (self, "%s: %s\n", _("error"), error->message);

      return dex_future_new_for_int (EXIT_FAILURE);
    }

  cancellable = foundry_command_line_dup_cancellable (self);

  return foundry_cli_command_run (command,
                                  self,
                                  (const char * const *)args,
                                  options,
                                  cancellable);
}

static void
foundry_command_line_class_init (FoundryCommandLineClass *klass)
{
  klass->run = foundry_command_line_real_run;

  g_dbus_error_register_error (FOUNDRY_COMMAND_LINE_ERROR,
                               FOUNDRY_COMMAND_LINE_ERROR_RUN_LOCAL,
                               "org.gnome.foundry.CommandLine.Error.RunLocal");
}

static void
foundry_command_line_init (FoundryCommandLine *self)
{
}

FoundryCommandLine *
foundry_command_line_new (void)
{
  return foundry_command_line_local_new ();
}

void
foundry_command_line_print (FoundryCommandLine *self,
                            const char         *format,
                            ...)
{
  g_autofree char *message = NULL;
  va_list args;

  g_return_if_fail (FOUNDRY_IS_COMMAND_LINE (self));
  g_return_if_fail (format != NULL);

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  FOUNDRY_COMMAND_LINE_GET_CLASS (self)->print (self, message);
}

void
foundry_command_line_printerr (FoundryCommandLine *self,
                               const char         *format,
                               ...)
{
  g_autofree char *message = NULL;
  va_list args;

  g_return_if_fail (FOUNDRY_IS_COMMAND_LINE (self));
  g_return_if_fail (format != NULL);

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  FOUNDRY_COMMAND_LINE_GET_CLASS (self)->printerr (self, message);
}

gboolean
foundry_command_line_isatty (FoundryCommandLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), FALSE);

  return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->isatty (self);
}

void
foundry_command_line_help (FoundryCommandLine *self)
{
  g_autoptr(GError) error = NULL;
  g_autofree char *help = NULL;
  FoundryCliCommandTree *tree;

  g_return_if_fail (FOUNDRY_IS_COMMAND_LINE (self));

  _foundry_init_plugins ();
  tree = foundry_cli_command_tree_get_default ();
  help = _foundry_cli_command_tree_get_help (tree,
                                             FOUNDRY_STRV_INIT ("foundry"),
                                             &error);

  if (help != NULL)
    foundry_command_line_print (self, "%s", help);
  else
    foundry_command_line_printerr (self, "%s: %s\n", _("error"), error->message);
}

/**
 * foundry_command_line_run:
 * @self: a #FoundryCommandLine
 * @argv: (array zero-terminated=1) (transfer none) (not nullable)
 *
 * Runs the command line.
 *
 * Returns: (transfer full): a #DexFuture that resolves to an int
 */
DexFuture *
foundry_command_line_run (FoundryCommandLine *self,
                          const char * const *argv)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), NULL);
  g_return_val_if_fail (argv != NULL, NULL);

  return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->run (self, argv);
}

char *
foundry_command_line_get_directory (FoundryCommandLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), NULL);

  return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->get_directory (self);
}

/**
 * foundry_command_line_get_environ:
 * @self: a #FoundryCommandLine
 *
 * Returns: (transfer full): the environment of the command line
 */
char **
foundry_command_line_get_environ (FoundryCommandLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), NULL);

  return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->get_environ (self);
}

gboolean
foundry_command_line_is_remote (FoundryCommandLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), FALSE);

  return FOUNDRY_IS_COMMAND_LINE_REMOTE (self);
}

DexFuture *
foundry_command_line_open (FoundryCommandLine *self,
                           int                 fd_number)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), NULL);

  if (fd_number < 0)
    return dex_future_new_reject (G_FILE_ERROR,
                                  G_FILE_ERROR_BADF,
                                  "Invalid fd number");

  return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->open (self, fd_number);
}

const char *
foundry_command_line_getenv (FoundryCommandLine *self,
                             const char         *name)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), NULL);

  return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->getenv (self, name);
}

int
foundry_command_line_get_stdin (FoundryCommandLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), -1);

  return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->get_stdin (self);
}

int
foundry_command_line_get_stdout (FoundryCommandLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), -1);

  return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->get_stdout (self);
}

int
foundry_command_line_get_stderr (FoundryCommandLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), -1);

  return FOUNDRY_COMMAND_LINE_GET_CLASS (self)->get_stderr (self);
}

void
foundry_command_line_print_object (FoundryCommandLine                 *self,
                                   GObject                            *object,
                                   const FoundryObjectSerializerEntry *entries,
                                   FoundryObjectSerializerFormat       format)
{
  g_autoptr(GListStore) store = NULL;

  g_return_if_fail (FOUNDRY_IS_COMMAND_LINE (self));
  g_return_if_fail (G_IS_OBJECT (object));

  store = g_list_store_new (G_OBJECT_TYPE (object));
  g_list_store_append (store, object);
  foundry_command_line_print_list (self, G_LIST_MODEL (store), entries, format, G_TYPE_INVALID);
}

typedef struct _Column
{
  const char *title;
  GParamSpec *pspec;
  gsize       longest;
  guint       is_boolean : 1;
  guint       is_number : 1;
  guint       is_enum : 1;
  guint       is_flags : 1;
  guint       is_strv : 1;
  guint       is_datetime : 1;
} Column;

static void
foundry_command_line_print_sized (FoundryCommandLine *self,
                                  gsize               size,
                                  const char         *message,
                                  gboolean            bold)
{
  gsize len = 0;

  if (message != NULL)
    {
      len = strlen (message);

      if (bold && foundry_command_line_isatty (self))
        foundry_command_line_print (self, "\e[1m%s\e[22m", message);
      else
        foundry_command_line_print (self, "%s", message);
    }

  for (; len < size; len++)
    foundry_command_line_print (self, " ");
}

static gboolean
is_number_type (GType type)
{
  switch ((int) type)
    {
    case G_TYPE_UINT:
    case G_TYPE_UINT64:
    case G_TYPE_INT:
    case G_TYPE_INT64:
    case G_TYPE_LONG:
    case G_TYPE_ULONG:
    case G_TYPE_DOUBLE:
    case G_TYPE_FLOAT:
      return TRUE;

    default:
      return FALSE;
    }
}

void
foundry_command_line_print_list (FoundryCommandLine                 *self,
                                 GListModel                         *model,
                                 const FoundryObjectSerializerEntry *entries,
                                 FoundryObjectSerializerFormat       format,
                                 GType                               expected_type)
{
  g_autofree Column *columns = NULL;
  g_autoptr(GTypeClass) klass = NULL;
  g_autoptr(GStringChunk) chunk = NULL;
  g_autoptr(GPtrArray) strings = NULL;
  GType item_type;
  guint n_items;
  guint n_columns;

  g_assert (FOUNDRY_IS_COMMAND_LINE (self));
  g_assert (!model || G_IS_LIST_MODEL (model));
  g_assert (entries != NULL);

  if (model == NULL)
    return;

  chunk = g_string_chunk_new (4096);
  strings = g_ptr_array_new ();

  if (expected_type != G_TYPE_INVALID)
    item_type = expected_type;
  else
    item_type = g_list_model_get_item_type (model);

  klass = g_type_class_ref (item_type);

  for (n_columns = 0; entries[n_columns].property; n_columns++);
  columns = g_new0 (Column, n_columns);

  for (guint c = 0; c < n_columns; c++)
    {
      columns[c].title = entries[c].heading;
      columns[c].longest = strlen (entries[c].heading);
      columns[c].pspec = g_object_class_find_property (G_OBJECT_CLASS (klass), entries[c].property);

      if (columns[c].pspec == NULL)
        {
          g_critical ("Object type %s does not have property '%s'",
                      g_type_name (item_type),
                      entries[c].property);
          return;
        }

      columns[c].is_datetime = g_type_is_a (columns[c].pspec->value_type, G_TYPE_DATE_TIME);
      columns[c].is_enum = G_TYPE_IS_ENUM (columns[c].pspec->value_type);
      columns[c].is_flags = G_TYPE_IS_FLAGS (columns[c].pspec->value_type);
      columns[c].is_strv = columns[c].pspec->value_type == G_TYPE_STRV;
      columns[c].is_boolean = columns[c].pspec->value_type == G_TYPE_BOOLEAN;
      columns[c].is_number = is_number_type (columns[c].pspec->value_type);
    }

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) object = g_list_model_get_item (model, i);

      for (guint ii = 0; entries[ii].property; ii++)
        {
          Column *column = &columns[ii];
          g_auto(GValue) value = G_VALUE_INIT;
          g_autofree char *tmpstr = NULL;
          const char *str;

          if (column->is_boolean)
            {
              g_value_init (&value, G_TYPE_BOOLEAN);
              g_object_get_property (object, entries[ii].property, &value);

              if (format == FOUNDRY_OBJECT_SERIALIZER_FORMAT_JSON)
                str = g_value_get_boolean (&value) ? "true" : "false";
              else
                {
                  if (g_value_get_boolean (&value))
                    str = g_string_chunk_insert_const (chunk, _("Yes"));
                  else
                    str = g_string_chunk_insert_const (chunk, _("No"));
                }
            }
          else if (column->is_datetime)
            {
              GDateTime *dt;

              g_value_init (&value, G_TYPE_DATE_TIME);
              g_object_get_property (object, entries[ii].property, &value);

              if ((dt = g_value_get_boxed (&value)))
                {
                  if (format == FOUNDRY_OBJECT_SERIALIZER_FORMAT_JSON)
                    tmpstr = g_date_time_format_iso8601 (dt);
                  else
                    tmpstr = g_date_time_format (dt, "%x %X");
                }

              str = tmpstr;
            }
          else if (column->is_enum)
            {
              g_autoptr(GEnumClass) enum_class = g_type_class_ref (column->pspec->value_type);
              GEnumValue *v;

              g_value_init (&value, column->pspec->value_type);
              g_object_get_property (object, entries[ii].property, &value);

              if ((v = g_enum_get_value (enum_class, g_value_get_enum (&value))))
                str = g_intern_string (v->value_nick);
              else
                str = NULL;
            }
          else if (column->is_flags)
            {
              g_autoptr(GFlagsClass) flags_class = g_type_class_ref (column->pspec->value_type);
              g_autoptr(GString) gstr = g_string_new (NULL);
              GFlagsValue *v;
              guint flags;

              g_value_init (&value, column->pspec->value_type);
              g_object_get_property (object, entries[ii].property, &value);

              flags = g_value_get_flags (&value);

              while ((v = g_flags_get_first_value (flags_class, flags)))
                {
                  if (gstr->len > 0)
                    g_string_append_c (gstr, '|');

                  g_string_append (gstr, v->value_nick);

                  flags &= ~v->value;

                  if (flags == 0)
                    break;
                }

              g_value_unset (&value);
              g_value_init (&value, G_TYPE_STRING);
              g_value_take_string (&value, g_string_free (g_steal_pointer (&gstr), FALSE));

              str = g_value_get_string (&value);
            }
          else if (column->is_strv)
            {
              g_auto(GStrv) strv = NULL;
              g_autoptr(GString) gstr = g_string_new (NULL);

              g_value_init (&value, G_TYPE_STRV);
              g_object_get_property (object, entries[ii].property, &value);

              if ((strv = g_value_dup_boxed (&value)))
                {
                  if (format == FOUNDRY_OBJECT_SERIALIZER_FORMAT_JSON)
                    g_string_append (gstr, "[");

                  for (guint z = 0; strv[z]; z++)
                    {
                      g_autofree char *escaped = g_strescape (strv[z], NULL);

                      if (format == FOUNDRY_OBJECT_SERIALIZER_FORMAT_JSON)
                        g_string_append_printf (gstr, "\"%s\"", escaped);
                      else
                        g_string_append (gstr, escaped);

                      if (strv[z+1] != NULL)
                        g_string_append (gstr, ", ");
                    }

                  if (format == FOUNDRY_OBJECT_SERIALIZER_FORMAT_JSON)
                    g_string_append (gstr, "]");
                }

              g_value_unset (&value);
              g_value_init (&value, G_TYPE_STRING);
              g_value_take_string (&value, g_string_free (g_steal_pointer (&gstr), FALSE));

              str = g_value_get_string (&value);
            }
          else
            {
              g_value_init (&value, G_TYPE_STRING);
              g_object_get_property (object, entries[ii].property, &value);

              str = g_value_get_string (&value);
            }

          if (str != NULL)
            {
              if (column->is_strv)
                {
                  column->longest = MAX (column->longest, strlen (str));
                  str = g_string_chunk_insert_const (chunk, str);
                }
              else
                {
                  g_autofree char *escaped = g_strescape (str, NULL);
                  column->longest = MAX (column->longest, strlen (escaped));
                  str = g_string_chunk_insert_const (chunk, escaped);
                }
            }

          g_ptr_array_add (strings, (char *)str);
        }
    }

  if (format == FOUNDRY_OBJECT_SERIALIZER_FORMAT_TEXT)
    {
      for (guint c = 0; c < n_columns; c++)
        {
          const Column *column = &columns[c];
          guint len = c + 1 == n_columns ? strlen (column->title) : column->longest;

          if (c > 0)
            foundry_command_line_print (self, "  ");

          foundry_command_line_print_sized (self, len, column->title, TRUE);
        }

      foundry_command_line_print (self, "\n");

      g_assert (strings->len % n_columns == 0);

      for (guint i = 0; i < n_items; i++)
        {
          for (guint ii = 0; ii < n_columns; ii++)
            {
              const Column *column = &columns[ii];
              const char *str = strings->pdata[i * n_columns + ii];

              if (ii > 0)
                foundry_command_line_print (self, "  ");

              foundry_command_line_print_sized (self, column->longest, str, FALSE);
            }

          foundry_command_line_print (self, "\n");
        }
    }
  else if (format == FOUNDRY_OBJECT_SERIALIZER_FORMAT_JSON)
    {
      foundry_command_line_print (self, "[");

      for (guint i = 0; i < n_items; i++)
        {
          if (i > 0)
            foundry_command_line_print (self, ", ");

          foundry_command_line_print (self, "{");

          for (guint ii = 0; ii < n_columns; ii++)
            {
              const Column *column = &columns[ii];
              const char *str = strings->pdata[i * n_columns + ii];

              if (ii > 0)
                foundry_command_line_print (self, ", ");

              foundry_command_line_print (self, "\"%s\": ", column->pspec->name);

              if (column->is_boolean || column->is_strv || column->is_number)
                foundry_command_line_print (self, "%s", str);
              else if (str == NULL)
                foundry_command_line_print (self, "null");
              else
                foundry_command_line_print (self, "\"%s\"", str);
            }

          foundry_command_line_print (self, "}");
        }

      foundry_command_line_print (self, "]\n");
    }
}

FoundryObjectSerializerFormat
foundry_object_serializer_format_parse (const char *string)
{
  static GEnumClass *klass;
  GEnumValue *value;

  if (string == NULL)
    return FOUNDRY_OBJECT_SERIALIZER_FORMAT_TEXT;

  if G_UNLIKELY (klass == NULL)
    klass = g_type_class_ref (FOUNDRY_TYPE_OBJECT_SERIALIZER_FORMAT);

  if ((value = g_enum_get_value_by_nick (klass, string)))
    return value->value;

  return FOUNDRY_OBJECT_SERIALIZER_FORMAT_TEXT;
}

void
foundry_command_line_clear_progress (FoundryCommandLine *self)
{
  g_return_if_fail (FOUNDRY_IS_COMMAND_LINE (self));

  if (foundry_command_line_isatty (self))
    foundry_command_line_print (self, "\033]9;4;0\e\\");
}

void
foundry_command_line_set_progress (FoundryCommandLine *self,
                                   guint               percent)
{
  g_return_if_fail (FOUNDRY_IS_COMMAND_LINE (self));

  if (foundry_command_line_isatty (self))
    foundry_command_line_print (self, "\033]9;4;1;%d\e\\", MIN (percent, 100));
}

void
foundry_command_line_clear_line (FoundryCommandLine *self)
{
  g_return_if_fail (FOUNDRY_IS_COMMAND_LINE (self));

  if (foundry_command_line_isatty (self))
    foundry_command_line_print (self, "\033[2K\r");
  else
    foundry_command_line_print (self, "\n");
}

void
foundry_command_line_set_title (FoundryCommandLine *self,
                                const char         *title)
{
  g_autofree char *escaped = NULL;
  g_autofree char *command = NULL;
  gsize len;

  g_return_if_fail (FOUNDRY_IS_COMMAND_LINE (self));

  if (title == NULL)
    escaped = g_strdup ("");
  else
    escaped = g_strescape (title, NULL);

  command = g_strdup_printf ("\e]0;%s\e\\", escaped);
  len = strlen (command);

  if (len != write (foundry_command_line_get_stdout (self), command, len))
    {
      /* Do Nothing */
    }
}

/**
 * foundry_command_line_dup_auth_provider:
 * @self: a [class@Foundry.CommandLine]
 *
 * Gets an auth provider for this command line client if possible.
 *
 * Currrently, this only returns an auth provider if the command line
 * client provides a TTY.
 *
 * Returns: (transfer full) (nullable):
 */
FoundryAuthProvider *
foundry_command_line_dup_auth_provider (FoundryCommandLine *self)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), NULL);

  if (foundry_command_line_isatty (self))
    return foundry_tty_auth_provider_new (foundry_command_line_get_stdin (self));

  return NULL;
}

/**
 * foundry_command_line_build_file_for_arg:
 * @self: a [class@Foundry.CommandLine]
 *
 * Returns: (transfer full):
 */
GFile *
foundry_command_line_build_file_for_arg (FoundryCommandLine *self,
                                         const char         *arg)
{
  g_return_val_if_fail (FOUNDRY_IS_COMMAND_LINE (self), NULL);
  g_return_val_if_fail (arg != NULL, NULL);

  if (g_path_is_absolute (arg))
    return g_file_new_for_path (arg);

  return g_file_new_build_filename (foundry_command_line_get_directory (self), arg, NULL);
}

/**
 * foundry_command_line_request_input:
 * @self: a [class@Foundry.CommandLine]
 * @input: a [class@Foundry.Input]
 *
 * Queries the user for the information requested in @input.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value or rejects with error.
 */
DexFuture *
foundry_command_line_request_input (FoundryCommandLine *self,
                                    FoundryInput       *input)
{
  int pty_fd;

  dex_return_error_if_fail (FOUNDRY_IS_COMMAND_LINE (self));
  dex_return_error_if_fail (FOUNDRY_IS_INPUT (input));

  pty_fd = foundry_command_line_get_stdout (self);

  return foundry_command_line_input (pty_fd, input);
}
