/*
 * test-acp-session-update.c
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
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <foundry.h>

#include "libfoundry/acp/foundry-acp-session-update-private.h"

static JsonNode *
parse_json (const char *json)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(JsonParser) parser = NULL;

  g_assert_nonnull (json);

  parser = json_parser_new ();
  json_parser_load_from_data (parser, json, -1, &error);
  g_assert_no_error (error);

  return json_node_ref (json_parser_get_root (parser));
}

static void
test_session_update_content_blocks (void)
{
  g_autoptr(FoundryAcpSessionUpdate) update = NULL;
  g_autoptr(FoundryAcpContentBlock) block = NULL;
  g_autoptr(GPtrArray) blocks = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autofree char *text = NULL;

  node = parse_json ("{"
                     "  \"update\": {"
                     "    \"sessionUpdate\": \"agent_message_chunk\","
                     "    \"content\": ["
                     "      { \"type\": \"text\", \"text\": \"hello\" },"
                     "      { \"type\": \"resource_link\", \"uri\": \"file:///tmp/a\","
                     "        \"name\": \"a\", \"mimeType\": \"text/plain\" }"
                     "    ]"
                     "  }"
                     "}");

  update = _foundry_acp_session_update_new_from_json ("session-1", node);
  g_assert_cmpint (foundry_acp_session_update_get_update_kind (update), ==,
                   FOUNDRY_ACP_SESSION_UPDATE_MESSAGE_CHUNK);

  text = foundry_acp_session_update_dup_text (update);
  g_assert_cmpstr (text, ==, "hello");

  blocks = foundry_acp_session_update_dup_content_blocks (update);
  g_assert_cmpuint (blocks->len, ==, 2);

  block = g_object_ref (g_ptr_array_index (blocks, 0));
  g_clear_pointer (&text, g_free);
  text = foundry_acp_content_block_dup_text (block);
  g_assert_cmpstr (text, ==, "hello");
}

static void
test_session_update_file_fields (void)
{
  g_autoptr(FoundryAcpSessionUpdate) update = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autofree char *edit_summary = NULL;
  g_autofree char *old_path = NULL;
  g_autofree char *path = NULL;
  gint64 byte_count = 0;
  guint begin_line = 0;
  guint end_line = 0;

  node = parse_json ("{"
                     "  \"sessionUpdate\": \"file_patch\","
                     "  \"path\": \"src/new.c\","
                     "  \"oldPath\": \"src/old.c\","
                     "  \"range\": { \"beginLine\": 4, \"endLine\": 9 },"
                     "  \"byteCount\": 128,"
                     "  \"editSummary\": \"+3 -1\""
                     "}");

  update = _foundry_acp_session_update_new_from_json ("session-1", node);
  g_assert_cmpint (foundry_acp_session_update_get_update_kind (update), ==,
                   FOUNDRY_ACP_SESSION_UPDATE_FILE_PATCH);

  path = foundry_acp_session_update_dup_path (update);
  old_path = foundry_acp_session_update_dup_old_path (update);
  edit_summary = foundry_acp_session_update_dup_edit_summary (update);

  g_assert_cmpstr (path, ==, "src/new.c");
  g_assert_cmpstr (old_path, ==, "src/old.c");
  g_assert_true (foundry_acp_session_update_get_line_range (update, &begin_line, &end_line));
  g_assert_cmpuint (begin_line, ==, 4);
  g_assert_cmpuint (end_line, ==, 9);
  g_assert_true (foundry_acp_session_update_get_byte_count (update, &byte_count));
  g_assert_cmpint (byte_count, ==, 128);
  g_assert_cmpstr (edit_summary, ==, "+3 -1");
}

static void
test_session_update_terminal_and_error_fields (void)
{
  g_autoptr(FoundryAcpSessionUpdate) error_update = NULL;
  g_autoptr(FoundryAcpSessionUpdate) terminal_update = NULL;
  g_autoptr(JsonNode) error_node = NULL;
  g_autoptr(JsonNode) terminal_node = NULL;
  g_autofree char *domain = NULL;
  g_autofree char *message = NULL;
  g_autofree char *progress = NULL;
  int error_code = 0;
  int exit_status = 0;

  terminal_node = parse_json ("{"
                              "  \"sessionUpdate\": \"terminal_exit\","
                              "  \"terminal\": {"
                              "    \"terminalId\": \"term-1\","
                              "    \"exitStatus\": 7,"
                              "    \"statusText\": \"finished\""
                              "  }"
                              "}");
  error_node = parse_json ("{"
                           "  \"sessionUpdate\": \"error\","
                           "  \"error\": {"
                           "    \"domain\": \"acp\","
                           "    \"code\": 42,"
                           "    \"message\": \"boom\""
                           "  }"
                           "}");

  terminal_update = _foundry_acp_session_update_new_from_json ("session-1", terminal_node);
  error_update = _foundry_acp_session_update_new_from_json ("session-1", error_node);

  g_assert_cmpint (foundry_acp_session_update_get_update_kind (terminal_update), ==,
                   FOUNDRY_ACP_SESSION_UPDATE_TERMINAL_EXITED);
  g_assert_true (foundry_acp_session_update_get_terminal_exit_status (terminal_update,
                                                                      &exit_status));
  g_assert_cmpint (exit_status, ==, 7);

  progress = foundry_acp_session_update_dup_progress_text (terminal_update);
  g_assert_cmpstr (progress, ==, "finished");

  g_assert_cmpint (foundry_acp_session_update_get_update_kind (error_update), ==,
                   FOUNDRY_ACP_SESSION_UPDATE_ERROR);
  g_assert_true (foundry_acp_session_update_get_error_code (error_update, &error_code));
  g_assert_cmpint (error_code, ==, 42);

  domain = foundry_acp_session_update_dup_error_domain (error_update);
  message = foundry_acp_session_update_dup_error_message (error_update);
  g_assert_cmpstr (domain, ==, "acp");
  g_assert_cmpstr (message, ==, "boom");
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/AcpSessionUpdate/content-blocks",
                   test_session_update_content_blocks);
  g_test_add_func ("/Foundry/AcpSessionUpdate/file-fields", test_session_update_file_fields);
  g_test_add_func ("/Foundry/AcpSessionUpdate/terminal-and-error-fields",
                   test_session_update_terminal_and_error_fields);

  return g_test_run ();
}
