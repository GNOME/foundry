/* plugin-sarif-diagnostic.c
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

#include "plugin-sarif-diagnostic.h"

#if 0
{
  "ruleId" : "error",
  "level" : "error",
  "message" : {
    "text" : "expected ‘;’ before ‘}}’ token"
  },
  "locations" : [
    {
      "physicalLocation" : {
        "artifactLocation" : {
          "uri" : "../../src/gcc/testsuite/g    cc.dg/missing-symbol-2.c",
          "uriBaseId" : "PWD"
        },
        "region" : {
          "startLine" : 35,
          "startColumn" : 12,
          "endColumn" : 13
        },
        "contextRegion" : {
          "startLine" : 35,
          "snippet" : {
            "text" : "  return 42 /* { dg-error \"expected ';'\" } */\n"
          }
        }
      },
      "logicalLocations" : [
        {
          "index" : 2,
          "fullyQualifiedName" : "missing_semicolon"
        }
      ]
    }
  ],
  "fixes" : [
    {
      "artifactChanges" : [
        {
          "artifactLocation" : {
            "uri" : "../../src/gcc/testsuite/gcc.dg/missing-symbol-2.c",
            "uriBaseId" : "PWD"
          },
          "replacements" : [
            {
              "deletedRegion" : {
                "startLine" : 35,
                "startCo    lumn" : 12,
                "endColumn" : 12
              },
              "insertedContent" : {
                "text" : ";"
              }
            }
          ]
        }
      ]
    }
  ]
}
#endif

/* This doesn't currently handle everything SARIF can do, but we
 * can certainly extend our diagnostic API to support more.
 *
 * Especially since our 1.0 doesn't have "fixit" support natively
 * and would need to be applied via "code actions".
 */

FoundryDiagnostic *
plugin_sarif_diagnostic_new (FoundryContext *context,
                             JsonNode       *result)
{
  g_autoptr(FoundryDiagnosticBuilder) builder = NULL;
  JsonNode *locations = NULL;
  const char *level = NULL;
  const char *text = NULL;
  const char *uri = NULL;
  const char *uri_base_id = NULL;
  const char *rule_id = NULL;
  const char *snippet_text = NULL;
  gint64 start_line = 0;
  gint64 start_column = 0;
  gint64 end_column = 0;
  gint64 context_start_line = 0;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (result != NULL, NULL);

  builder = foundry_diagnostic_builder_new (context);

  if (FOUNDRY_JSON_OBJECT_PARSE (result, "level", FOUNDRY_JSON_NODE_GET_STRING (&level)))
    {
      if (g_str_equal (level, "error"))
        foundry_diagnostic_builder_set_severity (builder, FOUNDRY_DIAGNOSTIC_ERROR);
      else if (g_str_equal (level, "warning"))
        foundry_diagnostic_builder_set_severity (builder, FOUNDRY_DIAGNOSTIC_WARNING);
      else if (g_str_equal (level, "note"))
        foundry_diagnostic_builder_set_severity (builder, FOUNDRY_DIAGNOSTIC_NOTE);
      else
        foundry_diagnostic_builder_set_severity (builder, FOUNDRY_DIAGNOSTIC_IGNORED);
    }

  if (FOUNDRY_JSON_OBJECT_PARSE (result, "ruleId", FOUNDRY_JSON_NODE_GET_STRING (&rule_id)))
    foundry_diagnostic_builder_set_rule_id (builder, rule_id);

  if (FOUNDRY_JSON_OBJECT_PARSE (result,
                                 "message", "{",
                                   "text", FOUNDRY_JSON_NODE_GET_STRING (&text),
                                 "}"))
    foundry_diagnostic_builder_set_message (builder, text);

  if (FOUNDRY_JSON_OBJECT_PARSE (result, "locations", FOUNDRY_JSON_NODE_GET_NODE (&locations)) &&
      JSON_NODE_HOLDS_ARRAY (locations))
    {
      JsonArray *ar = json_node_get_array (locations);
      guint length = json_array_get_length (ar);

      for (guint i = 0; i < length; i++)
        {
          JsonNode *location = json_array_get_element (ar, i);

          if (FOUNDRY_JSON_OBJECT_PARSE (location,
                                         "physicalLocation", "{",
                                           "artifactLocation", "{",
                                             "uri", FOUNDRY_JSON_NODE_GET_STRING (&uri),
                                             "uriBaseId", FOUNDRY_JSON_NODE_GET_STRING (&uri_base_id),
                                           "}",
                                           "region", "{",
                                             "startLine", FOUNDRY_JSON_NODE_GET_INT (&start_line),
                                             "startColumn", FOUNDRY_JSON_NODE_GET_INT (&start_column),
                                             "endColumn", FOUNDRY_JSON_NODE_GET_INT (&end_column),
                                           "}",
                                           "contextRegion", "{",
                                             "startLine", FOUNDRY_JSON_NODE_GET_INT (&context_start_line),
                                             "snippet", "{",
                                               "text", FOUNDRY_JSON_NODE_GET_STRING (&snippet_text),
                                             "}",
                                           "}",
                                         "}"))
            {
              if (i == 0)
                {
                  foundry_diagnostic_builder_set_line (builder, MAX (0, start_line - 1));
                  foundry_diagnostic_builder_set_line_offset (builder, MAX (0, start_column - 1));
                }

              foundry_diagnostic_builder_add_range (builder,
                                                    MAX (0, start_line - 1),
                                                    MAX (0, start_column - 1),
                                                    MAX (0, start_line),
                                                    MAX (0, end_column - 1));
            }
        }
    }

  return foundry_diagnostic_builder_end (builder);
}
