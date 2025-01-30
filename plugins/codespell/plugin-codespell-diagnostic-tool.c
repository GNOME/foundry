/* plugin-codespell-diagnostic-tool.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
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

#include "plugin-codespell-diagnostic-tool.h"

struct _PluginCodespellDiagnosticTool
{
  FoundryDiagnosticTool parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginCodespellDiagnosticTool, plugin_codespell_diagnostic_tool, FOUNDRY_TYPE_DIAGNOSTIC_TOOL)

static DexFuture *
plugin_codespell_diagnostic_tool_dup_bytes_for_stdin (FoundryDiagnosticTool *diagnostic_tool,
                                                      GFile                 *file,
                                                      GBytes                *contents,
                                                      const char            *laguage)
{
  g_assert (PLUGIN_IS_CODESPELL_DIAGNOSTIC_TOOL (diagnostic_tool));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file || contents);

  if (contents == NULL)
    return dex_file_load_contents_bytes (file);
  else
    return dex_future_new_take_boxed (G_TYPE_BYTES, g_bytes_ref (contents));
}

static DexFuture *
plugin_codespell_diagnostic_tool_extract_from_stdout (FoundryDiagnosticTool *diagnostic_tool,
                                                      GFile                 *file,
                                                      GBytes                *contents,
                                                      const char            *language,
                                                      GBytes                *stdout_bytes)
{
  static GRegex *regex;
  g_autoptr(FoundryDiagnosticBuilder) builder = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GListStore) diagnostics = NULL;
  g_autoptr(GMatchInfo) issues = NULL;
  const char *data;
  gsize len;

  g_assert (PLUGIN_IS_CODESPELL_DIAGNOSTIC_TOOL (diagnostic_tool));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file || contents);
  g_assert (stdout_bytes);

  if G_UNLIKELY (regex == NULL)
    {
      g_autoptr(GError) error = NULL;
      regex = g_regex_new ("(([0-9]+): .+?\n\t([a-zA-Z]+) ==> ([a-zA-Z0-9]+))",
                           G_REGEX_RAW,
                           G_REGEX_MATCH_NEWLINE_ANY,
                           &error);
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (diagnostic_tool));
  builder = foundry_diagnostic_builder_new (context);
  diagnostics = g_list_store_new (FOUNDRY_TYPE_DIAGNOSTIC);

  if (!(data = g_bytes_get_data (stdout_bytes, &len)) || !len)
    return dex_future_new_take_object (g_steal_pointer (&diagnostics));

  if (!g_regex_match_full (regex, data, len, 0, 0, &issues, NULL))
    return dex_future_new_take_object (g_steal_pointer (&diagnostics));

  g_assert (issues != NULL);

  while (g_match_info_matches (issues))
    {
      g_autofree char *line_word = g_match_info_fetch (issues, 2);
      g_autofree char *typo_word = g_match_info_fetch (issues, 3);
      g_autofree char *expected_word = g_match_info_fetch (issues, 4);
      guint64 lineno = g_ascii_strtoull (line_word, NULL, 10);

      if (lineno != 0 &&
          line_word != NULL &&
          typo_word != NULL &&
          expected_word != NULL)
        {
          g_autoptr(FoundryDiagnostic) diagnostic = NULL;

          foundry_diagnostic_builder_set_file (builder, file);
          foundry_diagnostic_builder_set_line (builder, lineno);
          foundry_diagnostic_builder_take_message (builder,
                                                   g_strdup_printf (_("Possible typo in “%s”. Did you mean “%s”?"),
                                                                    typo_word, expected_word));

          diagnostic = foundry_diagnostic_builder_end (builder);

          g_list_store_append (diagnostics, diagnostic);
        }

      if (!g_match_info_next (issues, NULL))
        break;
    }

  return dex_future_new_take_object (g_steal_pointer (&diagnostics));
}

static void
plugin_codespell_diagnostic_tool_constructed (GObject *object)
{
  g_autoptr(FoundryCommand) command = NULL;
  g_autoptr(FoundryContext) context = NULL;

  G_OBJECT_CLASS (plugin_codespell_diagnostic_tool_parent_class)->constructed (object);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (object));
  command = foundry_command_new (context);
  foundry_command_set_argv (command,
                            FOUNDRY_STRV_INIT ("codespell", "-"));
  foundry_diagnostic_tool_set_command (FOUNDRY_DIAGNOSTIC_TOOL (object), command);
}

static void
plugin_codespell_diagnostic_tool_class_init (PluginCodespellDiagnosticToolClass *klass)
{
  FoundryDiagnosticToolClass *diagnostic_tool_class = FOUNDRY_DIAGNOSTIC_TOOL_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = plugin_codespell_diagnostic_tool_constructed;

  diagnostic_tool_class->dup_bytes_for_stdin = plugin_codespell_diagnostic_tool_dup_bytes_for_stdin;
  diagnostic_tool_class->extract_from_stdout = plugin_codespell_diagnostic_tool_extract_from_stdout;
}

static void
plugin_codespell_diagnostic_tool_init (PluginCodespellDiagnosticTool *self)
{
}
