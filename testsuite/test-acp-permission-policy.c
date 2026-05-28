/*
 * test-acp-permission-policy.c
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <foundry.h>

#include "libfoundry/acp/foundry-acp-permission-request-private.h"

static FoundryAcpPermissionRequest *
new_request (const char *json)
{
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;

  g_assert_nonnull (json);

  parser = json_parser_new ();
  json_parser_load_from_data (parser, json, -1, &error);
  g_assert_no_error (error);

  return _foundry_acp_permission_request_new ("session-1", json_parser_get_root (parser));
}

static void
test_low_risk_default_allow (void)
{
  g_autoptr(FoundryAcpPermissionPolicy) policy = NULL;
  g_autoptr(FoundryAcpPermissionRequest) request = NULL;
  g_autoptr(FoundryAcpPermissionResponse) response = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autofree char *option_id = NULL;

  project_directory = g_file_new_for_path ("/tmp/project");
  policy = foundry_acp_permission_policy_new_for_project_directory (project_directory);
  request = new_request ("{"
                         "  \"path\": \"src/main.c\","
                         "  \"riskLevel\": \"low\","
                         "  \"defaultOption\": \"allow\","
                         "  \"options\": ["
                         "    {\"optionId\": \"allow\", \"label\": \"Allow\"},"
                         "    {\"optionId\": \"deny\", \"label\": \"Deny\"}"
                         "  ]"
                         "}");

  response = foundry_acp_permission_policy_decide (policy, request);
  option_id = foundry_acp_permission_response_dup_option_id (response);

  g_assert_false (foundry_acp_permission_response_get_cancelled (response));
  g_assert_cmpstr (option_id, ==, "allow");
}

static void
test_high_risk_cancels (void)
{
  g_autoptr(FoundryAcpPermissionPolicy) policy = NULL;
  g_autoptr(FoundryAcpPermissionRequest) request = NULL;
  g_autoptr(FoundryAcpPermissionResponse) response = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autofree char *option_id = NULL;

  project_directory = g_file_new_for_path ("/tmp/project");
  policy = foundry_acp_permission_policy_new_for_project_directory (project_directory);
  request = new_request ("{"
                         "  \"path\": \"src/main.c\","
                         "  \"riskLevel\": \"high\","
                         "  \"defaultOption\": \"allow\","
                         "  \"options\": ["
                         "    {\"optionId\": \"allow\", \"label\": \"Allow\"},"
                         "    {\"optionId\": \"deny\", \"label\": \"Deny\"}"
                         "  ]"
                         "}");

  response = foundry_acp_permission_policy_decide (policy, request);
  option_id = foundry_acp_permission_response_dup_option_id (response);

  g_assert_false (foundry_acp_permission_response_get_cancelled (response));
  g_assert_cmpstr (option_id, ==, "deny");
}

static void
test_outside_project_cancels (void)
{
  g_autoptr(FoundryAcpPermissionPolicy) policy = NULL;
  g_autoptr(FoundryAcpPermissionRequest) request = NULL;
  g_autoptr(FoundryAcpPermissionResponse) response = NULL;
  g_autoptr(GFile) project_directory = NULL;
  g_autofree char *option_id = NULL;

  project_directory = g_file_new_for_path ("/tmp/project");
  policy = foundry_acp_permission_policy_new_for_project_directory (project_directory);
  request = new_request ("{"
                         "  \"path\": \"/tmp/outside.txt\","
                         "  \"riskLevel\": \"low\","
                         "  \"defaultOption\": \"allow\","
                         "  \"options\": ["
                         "    {\"optionId\": \"allow\", \"label\": \"Allow\"},"
                         "    {\"optionId\": \"deny\", \"label\": \"Deny\"}"
                         "  ]"
                         "}");

  response = foundry_acp_permission_policy_decide (policy, request);
  option_id = foundry_acp_permission_response_dup_option_id (response);

  g_assert_false (foundry_acp_permission_response_get_cancelled (response));
  g_assert_cmpstr (option_id, ==, "deny");
}

static void
test_destructive_default_cancels (void)
{
  g_autoptr(FoundryAcpPermissionPolicy) policy = NULL;
  g_autoptr(FoundryAcpPermissionRequest) request = NULL;
  g_autoptr(FoundryAcpPermissionResponse) response = NULL;
  g_autoptr(GFile) project_directory = NULL;

  project_directory = g_file_new_for_path ("/tmp/project");
  policy = foundry_acp_permission_policy_new_for_project_directory (project_directory);
  request = new_request ("{"
                         "  \"path\": \"src/main.c\","
                         "  \"riskLevel\": \"low\","
                         "  \"defaultOption\": \"delete\","
                         "  \"options\": ["
                         "    {\"optionId\": \"delete\", \"label\": \"Delete\","
                         "     \"destructive\": true}"
                         "  ]"
                         "}");

  response = foundry_acp_permission_policy_decide (policy, request);

  g_assert_true (foundry_acp_permission_response_get_cancelled (response));
}

static void
test_contains_path (void)
{
  g_autoptr(FoundryAcpPermissionPolicy) policy = NULL;
  g_autoptr(GFile) project_directory = NULL;

  project_directory = g_file_new_for_path ("/tmp/project");
  policy = foundry_acp_permission_policy_new_for_project_directory (project_directory);

  g_assert_true (foundry_acp_permission_policy_contains_path (policy, "src/main.c"));
  g_assert_true (foundry_acp_permission_policy_contains_path (policy, "/tmp/project/src/main.c"));
  g_assert_false (foundry_acp_permission_policy_contains_path (policy, "/tmp/project-other/a.c"));
  g_assert_false (foundry_acp_permission_policy_contains_path (policy, "/tmp/outside.txt"));
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Foundry/AcpPermissionPolicy/low-risk-default-allow",
                   test_low_risk_default_allow);
  g_test_add_func ("/Foundry/AcpPermissionPolicy/high-risk-cancels",
                   test_high_risk_cancels);
  g_test_add_func ("/Foundry/AcpPermissionPolicy/outside-project-cancels",
                   test_outside_project_cancels);
  g_test_add_func ("/Foundry/AcpPermissionPolicy/destructive-default-cancels",
                   test_destructive_default_cancels);
  g_test_add_func ("/Foundry/AcpPermissionPolicy/contains-path", test_contains_path);

  return g_test_run ();
}
