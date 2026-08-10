/* foundry-cli-command-tree.c
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

#include <glib/gi18n-lib.h>
#include <libdex.h>
#include <libpeas.h>

#include "foundry-cli-command-private.h"
#include "foundry-cli-command-tree-addin-private.h"
#include "foundry-cli-command-tree-private.h"

/**
 * FoundryCliCommandTree:
 *
 * Manages a hierarchical tree of CLI commands.
 *
 * FoundryCliCommandTree provides functionality for organizing and managing
 * command line interface commands in a hierarchical structure. It supports
 * command registration, lookup, and provides efficient access to command
 * functionality for the CLI system.
 */

struct _FoundryCliCommandTree
{
  GObject           parent_instance;
  GNode            *root;
  PeasExtensionSet *addins;
};

typedef struct _FoundryCliCommandTreeData
{
  char              *name;
  char              *summary;
  char              *arguments;
  char              *gettext_package;
  FoundryCliCommand *command;
} FoundryCliCommandTreeData;

G_DEFINE_FINAL_TYPE (FoundryCliCommandTree, foundry_cli_command_tree, G_TYPE_OBJECT)

static gboolean
is_null_or_empty (const char *str)
{
  return str == NULL || str[0] == 0;
}

static gboolean
has_prefix_or_equal (const char *str,
                     const char *prefix)
{
  return *prefix == 0 || g_str_has_prefix (str, prefix) || g_str_equal (str, prefix);
}

static void
free_data (FoundryCliCommandTreeData *data)
{
  g_clear_pointer (&data->name, g_free);
  g_clear_pointer (&data->summary, g_free);
  g_clear_pointer (&data->arguments, g_free);
  g_clear_pointer (&data->gettext_package, g_free);
  g_clear_pointer (&data->command, foundry_cli_command_free);
  g_free (data);
}

static gboolean
free_command_traverse (GNode    *node,
                       gpointer  user_data)
{
  g_clear_pointer (&node->data, free_data);
  return FALSE;
}

static void
free_node (GNode *root)
{
  g_node_traverse (root, G_IN_ORDER, G_TRAVERSE_ALL, -1, free_command_traverse, NULL);
  g_node_destroy (root);
}

static void
foundry_cli_command_tree_addin_added_cb (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         GObject          *extension,
                                         gpointer          user_data)
{
  FoundryCliCommandTree *self = user_data;
  FoundryCliCommandTreeAddin *addin = FOUNDRY_CLI_COMMAND_TREE_ADDIN (extension);

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (FOUNDRY_IS_CLI_COMMAND_TREE (self));
  g_assert (FOUNDRY_IS_CLI_COMMAND_TREE_ADDIN (addin));

  dex_future_disown (_foundry_cli_command_tree_addin_load (addin, self));
}

static void
foundry_cli_command_tree_dispose (GObject *object)
{
  FoundryCliCommandTree *self = (FoundryCliCommandTree *)object;

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_cli_command_tree_parent_class)->dispose (object);
}

static void
foundry_cli_command_tree_finalize (GObject *object)
{
  FoundryCliCommandTree *self = (FoundryCliCommandTree *)object;

  g_clear_pointer (&self->root, free_node);

  G_OBJECT_CLASS (foundry_cli_command_tree_parent_class)->finalize (object);
}

static void
foundry_cli_command_tree_constructed (GObject *object)
{
  FoundryCliCommandTree *self = (FoundryCliCommandTree *)object;

  G_OBJECT_CLASS (foundry_cli_command_tree_parent_class)->constructed (object);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         FOUNDRY_TYPE_CLI_COMMAND_TREE_ADDIN,
                                         NULL);
  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (foundry_cli_command_tree_addin_added_cb),
                           self,
                           0);
  peas_extension_set_foreach (self->addins,
                              foundry_cli_command_tree_addin_added_cb,
                              self);
}

static void
foundry_cli_command_tree_class_init (FoundryCliCommandTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_cli_command_tree_constructed;
  object_class->dispose = foundry_cli_command_tree_dispose;
  object_class->finalize = foundry_cli_command_tree_finalize;
}

static void
foundry_cli_command_tree_init (FoundryCliCommandTree *self)
{
  self->root = g_node_new (NULL);
}

FoundryCliCommandTree *
foundry_cli_command_tree_new (void)
{
  return g_object_new (FOUNDRY_TYPE_CLI_COMMAND_TREE, NULL);
}

static GNode *
ensure_node (GNode              *node,
             const char * const *path)
{
  FoundryCliCommandTreeData *new_data;
  GNode *new_child;

  if (path[0] == NULL)
    return node;

  g_assert (path[0] != NULL);

  for (GNode *child = node->children; child != NULL; child = child->next)
    {
      FoundryCliCommandTreeData *child_data = child->data;

      g_assert (child_data != NULL);

      if (g_str_equal (child_data->name, path[0]))
        return ensure_node (child, &path[1]);
    }

  new_data = g_new0 (FoundryCliCommandTreeData, 1);
  new_data->name = g_strdup (path[0]);
  new_child = g_node_new (new_data);

  g_node_append (node, new_child);

  return ensure_node (new_child, &path[1]);
}

/**
 * foundry_cli_command_tree_register_full:
 * @self: a [class@Foundry.CliCommandTree]
 * @path: (array zero-terminated=1): the command path
 * @command: the command implementation
 * @summary: (nullable): a short summary of the command, or %NULL to preserve
 *   an existing group summary
 * @arguments: (nullable): the command's positional argument synopsis
 *
 * Registers @command at @path with separate help metadata.
 *
 * The @summary should be a short verb phrase without positional argument
 * syntax. The @arguments should contain only positional arguments because
 * the generated help adds option syntax automatically.
 *
 * Since: 1.2
 */
void
foundry_cli_command_tree_register_full (FoundryCliCommandTree   *self,
                                        const char * const      *path,
                                        const FoundryCliCommand *command,
                                        const char              *summary,
                                        const char              *arguments)
{
  FoundryCliCommandTreeData *data;
  GNode *node;

  g_return_if_fail (FOUNDRY_IS_CLI_COMMAND_TREE (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (path[0] != NULL);
  g_return_if_fail (command != NULL);

  node = ensure_node (self->root, path);
  data = node->data;

  g_assert (data != NULL);

  g_clear_pointer (&data->command, foundry_cli_command_free);
  g_clear_pointer (&data->arguments, g_free);

  data->command = foundry_cli_command_copy (command);
  data->arguments = g_strdup (arguments);

  /* Keep the legacy description available to callers which look up and run
   * commands directly instead of rendering help through the command tree.
   */
  if (summary != NULL)
    {
      g_free (data->summary);
      g_free (data->gettext_package);
      g_free ((char *)data->command->description);
      data->summary = g_strdup (summary);
      data->gettext_package = g_strdup (command->gettext_package);
      data->command->description = g_strdup (summary);
    }
  else if (data->summary != NULL)
    {
      g_free ((char *)data->command->description);
      data->command->description = g_strdup (data->summary);
    }
}

/**
 * foundry_cli_command_tree_register_group:
 * @self: a [class@Foundry.CliCommandTree]
 * @path: (array zero-terminated=1): the command group path
 * @gettext_package: (nullable): gettext package for @summary
 * @summary: a short summary of the command group
 *
 * Adds help metadata to a command group. The group may be registered before
 * or after its child commands and may also have a command of its own.
 *
 * Since: 1.2
 */
void
foundry_cli_command_tree_register_group (FoundryCliCommandTree *self,
                                         const char * const    *path,
                                         const char            *gettext_package,
                                         const char            *summary)
{
  FoundryCliCommandTreeData *data;
  GNode *node;

  g_return_if_fail (FOUNDRY_IS_CLI_COMMAND_TREE (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (path[0] != NULL);
  g_return_if_fail (summary != NULL);

  node = ensure_node (self->root, path);
  data = node->data;

  g_assert (data != NULL);

  g_free (data->summary);
  g_free (data->gettext_package);
  data->summary = g_strdup (summary);
  data->gettext_package = g_strdup (gettext_package);

  if (data->command != NULL)
    {
      g_free ((char *)data->command->description);
      data->command->description = g_strdup (summary);
    }
}

/**
 * foundry_cli_command_tree_register:
 * @self: a [class@Foundry.CliCommandTree]
 * @path: (array zero-terminated=1): the command path
 * @command: the command implementation
 *
 * Registers @command at @path.
 *
 * The command's description is used as its help summary. Use
 * [method@Foundry.CliCommandTree.register_full] when positional argument
 * syntax should be displayed in generated help.
 */
void
foundry_cli_command_tree_register (FoundryCliCommandTree   *self,
                                   const char * const      *path,
                                   const FoundryCliCommand *command)
{
  g_return_if_fail (FOUNDRY_IS_CLI_COMMAND_TREE (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (path[0] != NULL);
  g_return_if_fail (command != NULL);

  foundry_cli_command_tree_register_full (self,
                                          path,
                                          command,
                                          command->description,
                                          NULL);
}

static void
print_recurse (const GNode *node,
               int          depth)
{
  if (node->parent != NULL)
    {
      const FoundryCliCommandTreeData *data = node->data;
      for (int i = 0; i < depth; i++)
        g_print ("  ");
      g_print ("%s\n", data->name);
    }

  for (const GNode *child = node->children; child; child = child->next)
    print_recurse (child, depth + 1);
}

void
_foundry_cli_command_tree_print (FoundryCliCommandTree *self)
{
  g_return_if_fail (FOUNDRY_IS_CLI_COMMAND_TREE (self));

  print_recurse (self->root, -1);
}

static void
clear_entry_data (gpointer data)
{
  GOptionEntry *entry = data;

  switch (entry->arg)
    {
      case G_OPTION_ARG_NONE:
      case G_OPTION_ARG_INT:
      case G_OPTION_ARG_INT64:
      case G_OPTION_ARG_DOUBLE:
        break;

      case G_OPTION_ARG_FILENAME:
      case G_OPTION_ARG_STRING:
        g_clear_pointer ((char **)entry->arg_data, g_free);
        break;

      case G_OPTION_ARG_FILENAME_ARRAY:
      case G_OPTION_ARG_STRING_ARRAY:
        g_clear_pointer ((char ***)entry->arg_data, g_strfreev);
        break;

      case G_OPTION_ARG_CALLBACK:
      default:
        g_assert_not_reached ();
    }

  g_clear_pointer (&entry->arg_data, g_free);
}

static GNode *
lookup_recurse (GNode               *node,
                gboolean             for_completion,
                char              ***args,
                FoundryCliOptions   *options,
                GError             **error)
{
  const char * const *argv = (const char * const *)*args;
  FoundryCliCommandTreeData *data;

  g_assert (argv != NULL);
  g_assert (argv[0] != NULL);

  if (node == NULL)
    return NULL;

  data = node->data;

  if (data->command != NULL && data->command->options != NULL)
    {
      g_autoptr(GOptionContext) context = g_option_context_new (NULL);
      g_autoptr(GArray) entries = g_array_new (TRUE, TRUE, sizeof (GOptionEntry));
      g_autoptr(GError) local_error = NULL;

      g_array_set_clear_func (entries, clear_entry_data);

      g_option_context_set_strict_posix (context, TRUE);
      g_option_context_set_help_enabled (context, FALSE);
      g_option_context_set_ignore_unknown_options (context, for_completion);

      for (const GOptionEntry *entry = data->command->options;
           entry->long_name != NULL;
           entry++)
        {
          GOptionEntry copy = *entry;

          if (entry->arg == G_OPTION_ARG_CALLBACK)
            {
              g_critical ("G_OPTION_ARG_CALLBACK is not supported");
              continue;
            }

          switch (entry->arg)
            {
            case G_OPTION_ARG_NONE:
              copy.arg_data = g_new (gboolean, 1);
              *(gboolean *)copy.arg_data = -1;
              break;

            case G_OPTION_ARG_INT:
              copy.arg_data = g_new (int, 1);
              *(int *)copy.arg_data = 0;
              break;

            case G_OPTION_ARG_INT64:
              copy.arg_data = g_new (gint64, 1);
              *(gint64 *)copy.arg_data = 0;
              break;

            case G_OPTION_ARG_DOUBLE:
              copy.arg_data = g_new (double, 1);
              *(double *)copy.arg_data = .0;
              break;

            case G_OPTION_ARG_FILENAME:
            case G_OPTION_ARG_FILENAME_ARRAY:
            case G_OPTION_ARG_STRING:
            case G_OPTION_ARG_STRING_ARRAY:
              copy.arg_data = g_new0 (char *, 1);
              break;

            case G_OPTION_ARG_CALLBACK:
            default:
              g_assert_not_reached ();
            }

          g_array_append_val (entries, copy);
        }

      if (entries->len > 0)
        g_option_context_add_main_entries (context,
                                           (const GOptionEntry *)(gpointer)entries->data,
                                           data->command->gettext_package ?
                                             data->command->gettext_package :
                                             GETTEXT_PACKAGE);

      if (data->command->prepare != NULL)
        data->command->prepare (context);

      if (!g_option_context_parse_strv (context, args, &local_error))
        {
          if (!for_completion ||
              !g_error_matches (local_error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE))
            {
              g_propagate_error (error, g_steal_pointer (&local_error));
              return NULL;
            }

          g_clear_error (&local_error);
        }

      argv = (const char * const *)*args;

      g_assert (args != NULL);
      g_assert (args[0] != NULL);

      for (guint i = 0; i < entries->len; i++)
        {
          const GOptionEntry *entry = &g_array_index (entries, GOptionEntry, i);

          switch (entry->arg)
            {
            case G_OPTION_ARG_NONE:
              if (*(gboolean *)entry->arg_data != -1)
                foundry_cli_options_set_boolean (options,
                                                 entry->long_name,
                                                 *(gboolean *)entry->arg_data);
              break;

            case G_OPTION_ARG_INT:
              if (*(int *)entry->arg_data != 0)
                foundry_cli_options_set_int (options,
                                             entry->long_name,
                                             *(int *)entry->arg_data);
              break;

            case G_OPTION_ARG_INT64:
              if (*(gint64 *)entry->arg_data != 0)
                foundry_cli_options_set_int64 (options,
                                               entry->long_name,
                                               *(gint64 *)entry->arg_data);
              break;

            case G_OPTION_ARG_DOUBLE:
              if (*(double *)entry->arg_data != .0)
                foundry_cli_options_set_double (options,
                                                entry->long_name,
                                                *(double *)entry->arg_data);
              break;

            case G_OPTION_ARG_FILENAME:
              if (*(const char **)entry->arg_data != NULL)
                foundry_cli_options_set_filename (options,
                                                  entry->long_name,
                                                  *(const char **)entry->arg_data);
              break;

            case G_OPTION_ARG_FILENAME_ARRAY:
              if (*(const char * const **)entry->arg_data != NULL)
                foundry_cli_options_set_filename_array (options,
                                                        entry->long_name,
                                                        *(const char * const **)entry->arg_data);
              break;

            case G_OPTION_ARG_STRING:
              if (*(const char **)entry->arg_data != NULL)
                foundry_cli_options_set_string (options,
                                                entry->long_name,
                                                *(const char **)entry->arg_data);
              break;

            case G_OPTION_ARG_STRING_ARRAY:
              if (*(const char * const **)entry->arg_data != NULL)
                foundry_cli_options_set_string_array (options,
                                                      entry->long_name,
                                                      *(const char * const **)entry->arg_data);
              break;

            case G_OPTION_ARG_CALLBACK:
            default:
              g_assert_not_reached ();
            }
        }
    }

  g_assert (argv != NULL);
  g_assert (argv[0] != NULL);
  g_assert (argv == (const char * const *)*args);

  if (argv[1] != NULL && argv[1][0] != '-')
    {
      for (GNode *child = node->children; child; child = child->next)
        {
          const FoundryCliCommandTreeData *child_data = child->data;

          g_assert (child_data != NULL);
          g_assert (child_data->name != NULL);

          if (g_str_equal (child_data->name, argv[1]))
            {
              g_auto(GStrv) new_args = g_strdupv ((char **)&argv[1]);
              GNode *ret;

              g_free (new_args[0]);
              new_args[0] = g_strdup_printf ("%s-%s", argv[0], argv[1]);

              if ((ret = lookup_recurse (child, for_completion, &new_args, options, error)))
                {
                  g_strfreev (*args);
                  *args = g_steal_pointer (&new_args);
                }

              return ret;
            }
        }
    }

  return node;
}

G_GNUC_WARN_UNUSED_RESULT
static char **
truncate_strv (char  **strv,
               gsize   len)
{
  for (gsize i = len; strv[i]; i++)
    g_clear_pointer (&strv[i], g_free);
  return strv;
}

G_GNUC_WARN_UNUSED_RESULT
static char **
join_strv (char **first,
           char **second)
{
  char **res = g_new0 (char *, g_strv_length (first) + g_strv_length (second) + 1);
  gsize j = 0;

  for (gsize i = 0; first[i]; i++)
    res[j++] = g_steal_pointer (&first[i]);

  for (gsize i = 0; second[i]; i++)
    res[j++] = g_steal_pointer (&second[i]);

  res[j] = NULL;

  g_free (first);
  g_free (second);

  return res;
}


static GNode *
foundry_cli_command_tree_lookup_full (FoundryCliCommandTree   *self,
                                      gboolean                 for_completion,
                                      char                  ***args,
                                      FoundryCliOptions      **options,
                                      GError                 **error)
{
  g_autoptr(FoundryCliOptions) parsed = foundry_cli_options_new ();
  g_auto(GStrv) suffix = NULL;
  GNode *node;

  /* If we come across a "--", strip everything starting from that and then
   * we'll join it at the end. We don't want any internal processing to
   * take that into account.
   */
  for (guint i = 0; (*args)[i]; i++)
    {
      if (g_str_equal ((*args)[i], "--"))
        {
          suffix = g_strdupv (&(*args)[i]);
          *args = truncate_strv (*args, i);
          break;
        }
    }

  if ((node = lookup_recurse (self->root->children, for_completion, args, parsed, error)))
    *options = g_steal_pointer (&parsed);

  if (suffix != NULL)
    *args = join_strv (*args, g_steal_pointer (&suffix));

  return node;
}

static char *
get_node_command (GNode *node)
{
  g_autoptr(GPtrArray) names = g_ptr_array_new ();
  g_autoptr(GString) command = g_string_new (NULL);

  g_assert (node != NULL);

  for (GNode *iter = node; iter != NULL && iter->data != NULL; iter = iter->parent)
    {
      const FoundryCliCommandTreeData *data = iter->data;

      g_ptr_array_add (names, data->name);
    }

  for (guint i = names->len; i > 0; i--)
    {
      if (command->len > 0)
        g_string_append_c (command, ' ');

      g_string_append (command, g_ptr_array_index (names, i - 1));
    }

  return g_string_free (g_steal_pointer (&command), FALSE);
}

static gboolean
is_help_entry (const GOptionEntry *entry)
{
  g_assert (entry != NULL);

  return g_strcmp0 (entry->long_name, "help") == 0;
}

static gboolean
command_has_options (const FoundryCliCommand *command)
{
  if (command == NULL || command->options == NULL)
    return FALSE;

  for (const GOptionEntry *entry = command->options;
       entry->long_name != NULL;
       entry++)
    {
      if (!(entry->flags & G_OPTION_FLAG_HIDDEN) && !is_help_entry (entry))
        return TRUE;
    }

  return FALSE;
}

static char *
format_option_name (const GOptionEntry *entry,
                    const char         *gettext_package)
{
  g_autofree char *suffix = NULL;

  g_assert (entry != NULL);
  g_assert (entry->long_name != NULL);

  if (entry->arg != G_OPTION_ARG_NONE)
    {
      const char *arg_description = entry->arg_description;

      if (arg_description == NULL)
        arg_description = N_("VALUE");

      suffix = g_strdup_printf ("=%s", g_dgettext (gettext_package, arg_description));
    }

  if (entry->short_name != 0)
    return g_strdup_printf ("-%c, --%s%s",
                            entry->short_name,
                            entry->long_name,
                            suffix ? suffix : "");

  return g_strdup_printf ("    --%s%s", entry->long_name, suffix ? suffix : "");
}

static void
append_options_help (GString                 *help,
                     const FoundryCliCommand *command)
{
  const char *gettext_package = GETTEXT_PACKAGE;
  gsize longest = strlen ("-h, --help");

  g_assert (help != NULL);

  if (command != NULL && command->gettext_package != NULL)
    gettext_package = command->gettext_package;

  if (command != NULL && command->options != NULL)
    {
      for (const GOptionEntry *entry = command->options;
           entry->long_name != NULL;
           entry++)
        {
          g_autofree char *name = NULL;

          if ((entry->flags & G_OPTION_FLAG_HIDDEN) || is_help_entry (entry))
            continue;

          name = format_option_name (entry, gettext_package);
          longest = MAX (longest, strlen (name));
        }
    }

  g_string_append_printf (help, "%s:\n", _("Options"));
  g_string_append_printf (help,
                          "  %-*s  %s\n",
                          (int)longest,
                          "-h, --help",
                          _("Show help"));

  if (command != NULL && command->options != NULL)
    {
      for (const GOptionEntry *entry = command->options;
           entry->long_name != NULL;
           entry++)
        {
          g_autofree char *name = NULL;
          const char *description;

          if ((entry->flags & G_OPTION_FLAG_HIDDEN) || is_help_entry (entry))
            continue;

          name = format_option_name (entry, gettext_package);
          description = entry->description;

          g_string_append_printf (help,
                                  "  %-*s",
                                  (int)longest,
                                  name);

          if (description != NULL)
            g_string_append_printf (help,
                                    "  %s",
                                    g_dgettext (gettext_package, description));

          g_string_append_c (help, '\n');
        }
    }
}

static void
append_commands_help (GString *help,
                      GNode   *node)
{
  gsize longest = 0;

  g_assert (help != NULL);
  g_assert (node != NULL);

  for (const GNode *child = node->children; child != NULL; child = child->next)
    {
      const FoundryCliCommandTreeData *data = child->data;

      longest = MAX (longest, strlen (data->name));
    }

  g_string_append_printf (help, "%s:\n", _("Commands"));

  for (const GNode *child = node->children; child != NULL; child = child->next)
    {
      const FoundryCliCommandTreeData *data = child->data;
      const char *gettext_package = data->gettext_package;

      if (gettext_package == NULL)
        gettext_package = GETTEXT_PACKAGE;

      if (data->summary != NULL)
        g_string_append_printf (help,
                                "  %-*s  %s",
                                (int)longest,
                                data->name,
                                g_dgettext (gettext_package, data->summary));
      else
        g_string_append_printf (help, "  %s", data->name);

      g_string_append_c (help, '\n');
    }
}

char *
_foundry_cli_command_tree_get_help (FoundryCliCommandTree  *self,
                                    const char * const     *argv,
                                    GError               **error)
{
  g_autoptr(FoundryCliOptions) options = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GString) help = NULL;
  g_autofree char *command_name = NULL;
  g_auto(GStrv) args = NULL;
  const FoundryCliCommandTreeData *data;
  GNode *node;

  g_return_val_if_fail (FOUNDRY_IS_CLI_COMMAND_TREE (self), NULL);
  g_return_val_if_fail (argv != NULL, NULL);
  g_return_val_if_fail (argv[0] != NULL, NULL);

  args = g_strdupv ((char **)argv);

  if (!(node = foundry_cli_command_tree_lookup_full (self,
                                                     TRUE,
                                                     &args,
                                                     &options,
                                                     &local_error)))
    {
      if (local_error != NULL)
        g_propagate_error (error, g_steal_pointer (&local_error));
      else
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_NOT_SUPPORTED,
                     _("No such command"));

      return NULL;
    }

  data = node->data;
  command_name = get_node_command (node);

  if ((data->command == NULL || data->command->run == NULL) && args[1] != NULL)
    {
      if (args[1][0] == '-')
        g_set_error (error,
                     G_OPTION_ERROR,
                     G_OPTION_ERROR_UNKNOWN_OPTION,
                     _("Unknown option %s for “%s”"),
                     args[1],
                     command_name);
      else
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_NOT_SUPPORTED,
                     _("Unknown command “%s” for “%s”"),
                     args[1],
                     command_name);

      return NULL;
    }

  help = g_string_new (NULL);
  g_string_append_printf (help, "%s:\n  %s", _("Usage"), command_name);

  if (command_has_options (data->command))
    g_string_append_printf (help, " [%s…]", _("OPTIONS"));

  if (node->children != NULL)
    g_string_append_printf (help, " %s", _("COMMAND"));

  if (data->arguments != NULL)
    {
      const char *gettext_package = data->gettext_package;

      if (gettext_package == NULL)
        gettext_package = GETTEXT_PACKAGE;

      g_string_append_printf (help,
                              " %s",
                              g_dgettext (gettext_package, data->arguments));
    }

  g_string_append_c (help, '\n');

  if (data->summary != NULL)
    {
      const char *gettext_package = data->gettext_package;

      if (gettext_package == NULL)
        gettext_package = GETTEXT_PACKAGE;

      g_string_append_printf (help,
                              "\n%s\n",
                              g_dgettext (gettext_package, data->summary));
    }

  if (node->children != NULL)
    {
      g_string_append_c (help, '\n');
      append_commands_help (help, node);
    }

  g_string_append_c (help, '\n');
  append_options_help (help, data->command);

  return g_string_free (g_steal_pointer (&help), FALSE);
}

/**
 * foundry_cli_command_tree_lookup:
 * @args: (inout) (array zero-terminated=1) (not optional)
 */
const FoundryCliCommand *
foundry_cli_command_tree_lookup (FoundryCliCommandTree   *self,
                                 char                  ***args,
                                 FoundryCliOptions      **options,
                                 GError                 **error)
{
  GNode *node;

  g_return_val_if_fail (FOUNDRY_IS_CLI_COMMAND_TREE (self), NULL);
  g_return_val_if_fail (args != NULL, NULL);
  g_return_val_if_fail (options != NULL, NULL);

  if ((node = foundry_cli_command_tree_lookup_full (self, FALSE, args, options, error)))
    {
      const FoundryCliCommandTreeData *data = node->data;

      if (data->command != NULL)
        return data->command;
    }

  if (error && *error == NULL)
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_NOT_SUPPORTED,
                 _("No such command"));

  return NULL;
}

static const GOptionEntry *
find_entry (const GOptionEntry *entries,
            const char         *arg)
{
  if (arg == NULL)
    return NULL;

  if (arg[0] != '-')
    return NULL;

  /* Commonly used as a separator */
  if (g_str_equal (arg, "--"))
    return NULL;

  if (arg[1] == '-')
    {
      for (guint i = 0; entries[i].long_name; i++)
        {
          if (g_str_equal (&arg[2], entries[i].long_name))
            return &entries[i];
        }
    }
  else if (arg[1] != 0)
    {
      const char *next = g_utf8_next_char (arg);
      gunichar ch = g_utf8_get_char (next);

      if (*g_utf8_next_char (next) == 0)
        {
          for (guint i = 0; entries[i].long_name; i++)
            {
              if (ch == entries[i].short_name)
                return &entries[i];
            }
        }
    }

  return NULL;
}

/**
 * foundry_cli_command_tree_complete:
 * @self: a [class@Foundry.CliCommandTree]
 *
 * Returns: (transfer full):
 */
char **
foundry_cli_command_tree_complete (FoundryCliCommandTree *self,
                                   FoundryCommandLine    *command_line,
                                   const char            *line,
                                   int                    point,
                                   const char            *current)
{
  const FoundryCliCommandTreeData *data;
  g_autoptr(FoundryCliOptions) options = NULL;
  g_autoptr(GStrvBuilder) results = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *to_point = NULL;
  g_auto(GStrv) argv = NULL;
  GNode *node = NULL;
  int argc;

  g_return_val_if_fail (FOUNDRY_IS_CLI_COMMAND_TREE (self), NULL);
  g_return_val_if_fail (line != NULL, NULL);

  if (point < 0 || point > strlen (line))
    point = strlen (line);

  to_point = g_strndup (line, point);

  if (!g_shell_parse_argv (to_point, &argc, &argv, NULL))
    return NULL;

  /* ignore_unknown_options will strip things like "--" out, so we have
   * to use @current to potentially complete those.
   */
  if (!(node = foundry_cli_command_tree_lookup_full (self, TRUE, &argv, &options, &error)))
    return NULL;

  g_assert (argv != NULL);
  g_assert (argv[0] != NULL);
  g_assert (options != NULL);
  g_assert (node != NULL);

  argc = g_strv_length (argv);
  data = node->data;
  results = g_strv_builder_new ();

  if (data->command != NULL && data->command->complete)
    {
      g_auto(GStrv) completions = data->command->complete (command_line, argv[0], NULL, options, (const char * const *)argv, current);

      if (completions != NULL)
        g_strv_builder_addv (results, (const char **)completions);
    }

  if (argv[1] == NULL && g_strcmp0 (data->name, current) == 0)
    {
      g_autofree char *with_space = g_strdup_printf ("%s ", data->name);
      g_strv_builder_add (results, with_space);
      return g_strv_builder_end (results);
    }

  for (GNode *child = node->children; child; child = child->next)
    {
      const FoundryCliCommandTreeData *child_data = child->data;

      /* If just starting command name or completing existing command name,
       * then this command is a potential completion.
       */
      if (argv[1] == NULL ||
          (argv[2] == NULL &&
           has_prefix_or_equal (child_data->name, argv[1])))
        {
          g_autofree char *with_space = g_strdup_printf ("%s ", child_data->name);
          g_strv_builder_add (results, with_space);
        }
    }

  if (current != NULL &&
      data->command != NULL &&
      data->command->options != NULL)
    {
      /* Try to complete long commands */
      if (has_prefix_or_equal (current, "-"))
        {
          const char *name = current[1] ? &current[2] : "";

          for (guint i = 0; data->command->options[i].long_name; i++)
            {
              const GOptionEntry *entry = &data->command->options[i];

              if (is_help_entry (entry))
                continue;

              if (has_prefix_or_equal (entry->long_name, name))
                {
                  gboolean has_value = entry->arg != G_OPTION_ARG_NONE;
                  g_autofree char *dashed = g_strdup_printf ("--%s%s", entry->long_name, has_value ? "=" : " ");
                  g_strv_builder_add (results, dashed);
                }
            }
        }

      /* Try to complete short commands */
      if (has_prefix_or_equal (current, "-") && current[1] != '-')
        {
          gunichar ch = g_utf8_get_char (g_utf8_next_char (current));

          for (guint i = 0; data->command->options[i].long_name; i++)
            {
              const GOptionEntry *entry = &data->command->options[i];

              if (is_help_entry (entry) || entry->short_name == 0)
                continue;

              if (!ch || entry->short_name == ch)
                {
                  g_autofree char *dashed = g_strdup_printf ("-%c", entry->short_name);
                  g_strv_builder_add (results, dashed);
                }
            }
        }
    }

  if (current != NULL && current[0] == '-')
    {
      if (has_prefix_or_equal ("--help", current))
        g_strv_builder_add (results, "--help ");

      if (has_prefix_or_equal ("-h", current))
        g_strv_builder_add (results, "-h ");
    }

  /* If the final argv element is a switch, then find the type of it
   * and see if it takes an argument we know about.
   */
  if (is_null_or_empty (current) &&
      !is_null_or_empty (argv[argc-1]) &&
      argv[argc-1][0] == '-')
    {
      const GOptionEntry *entry;

      if ((entry = find_entry (data->command->options, argv[argc-1])))
        {
          if (entry->arg == G_OPTION_ARG_FILENAME ||
              entry->arg == G_OPTION_ARG_FILENAME_ARRAY)
            {
              g_strv_builder_add (results, "__FOUNDRY_FILE");
            }
          else if (entry->arg == G_OPTION_ARG_STRING ||
                   entry->arg == G_OPTION_ARG_STRING_ARRAY)
            {
              if (data->command->complete)
                {
                  g_auto(GStrv) completions = data->command->complete (command_line, argv[0], entry, options, (const char * const *)argv, current);

                  if (completions != NULL)
                    g_strv_builder_addv (results, (const char **)completions);
                }
            }
        }
    }

  return g_strv_builder_end (results);
}

/**
 * foundry_cli_command_tree_get_default:
 *
 * Gets the default instance for use by the foundry CLI tool.
 *
 * Returns: (transfer none): a #FoundryCliCommandTree
 */
FoundryCliCommandTree *
foundry_cli_command_tree_get_default (void)
{
  static FoundryCliCommandTree *instance;

  if (g_once_init_enter (&instance))
    g_once_init_leave (&instance, foundry_cli_command_tree_new ());

  return instance;
}
