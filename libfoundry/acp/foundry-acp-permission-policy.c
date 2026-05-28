/* foundry-acp-permission-policy.c
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-acp-permission-option.h"
#include "foundry-acp-permission-policy.h"
#include "foundry-acp-permission-request.h"
#include "foundry-acp-permission-response.h"
#include "foundry-context.h"

struct _FoundryAcpPermissionPolicy
{
  GObject parent_instance;
  GFile *project_directory;
};

G_DEFINE_FINAL_TYPE (FoundryAcpPermissionPolicy, foundry_acp_permission_policy, G_TYPE_OBJECT)

static void
foundry_acp_permission_policy_finalize (GObject *object)
{
  FoundryAcpPermissionPolicy *self = (FoundryAcpPermissionPolicy *)object;

  g_clear_object (&self->project_directory);

  G_OBJECT_CLASS (foundry_acp_permission_policy_parent_class)->finalize (object);
}

static void
foundry_acp_permission_policy_class_init (FoundryAcpPermissionPolicyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_permission_policy_finalize;
}

static void
foundry_acp_permission_policy_init (FoundryAcpPermissionPolicy *self)
{
}

static gboolean
str_equal_fold (const char *str,
                const char *expected)
{
  g_autofree char *down = NULL;

  if (str == NULL || expected == NULL)
    return FALSE;

  down = g_utf8_strdown (str, -1);

  return g_str_equal (down, expected);
}

static gboolean
risk_is_low (const char *risk)
{
  if (risk == NULL)
    return FALSE;

  return str_equal_fold (risk, "none") ||
         str_equal_fold (risk, "safe") ||
         str_equal_fold (risk, "low");
}

static gboolean
risk_requires_prompt (const char *risk)
{
  if (risk == NULL)
    return TRUE;

  return str_equal_fold (risk, "medium") ||
         str_equal_fold (risk, "high") ||
         str_equal_fold (risk, "critical") ||
         str_equal_fold (risk, "destructive");
}

static gboolean
option_matches_allow (FoundryAcpPermissionOption *option)
{
  g_autofree char *id = NULL;
  g_autofree char *label = NULL;

  g_assert (FOUNDRY_IS_ACP_PERMISSION_OPTION (option));

  if (foundry_acp_permission_option_get_destructive (option))
    return FALSE;

  id = foundry_acp_permission_option_dup_id (option);
  label = foundry_acp_permission_option_dup_label (option);

  return str_equal_fold (id, "allow") ||
         str_equal_fold (id, "approve") ||
         str_equal_fold (id, "yes") ||
         str_equal_fold (id, "proceed") ||
         str_equal_fold (label, "allow") ||
         str_equal_fold (label, "approve") ||
         str_equal_fold (label, "yes") ||
         str_equal_fold (label, "proceed");
}

static gboolean
option_matches_cancel (FoundryAcpPermissionOption *option)
{
  g_autofree char *id = NULL;
  g_autofree char *label = NULL;

  g_assert (FOUNDRY_IS_ACP_PERMISSION_OPTION (option));

  id = foundry_acp_permission_option_dup_id (option);
  label = foundry_acp_permission_option_dup_label (option);

  return str_equal_fold (id, "cancel") ||
         str_equal_fold (id, "deny") ||
         str_equal_fold (id, "reject") ||
         str_equal_fold (id, "no") ||
         str_equal_fold (label, "cancel") ||
         str_equal_fold (label, "deny") ||
         str_equal_fold (label, "reject") ||
         str_equal_fold (label, "no");
}

static FoundryAcpPermissionOption *
find_option_by_id (GListModel *options,
                   const char *option_id)
{
  guint n_items;

  g_assert (G_IS_LIST_MODEL (options));

  if (option_id == NULL)
    return NULL;

  n_items = g_list_model_get_n_items (options);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpPermissionOption) option = NULL;
      g_autofree char *id = NULL;

      option = g_list_model_get_item (options, i);
      id = foundry_acp_permission_option_dup_id (option);

      if (g_strcmp0 (id, option_id) == 0)
        return g_object_ref (option);
    }

  return NULL;
}

static FoundryAcpPermissionOption *
find_option_matching (GListModel *options,
                      gboolean  (*match) (FoundryAcpPermissionOption *option))
{
  guint n_items;

  g_assert (G_IS_LIST_MODEL (options));
  g_assert (match != NULL);

  n_items = g_list_model_get_n_items (options);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryAcpPermissionOption) option = NULL;

      option = g_list_model_get_item (options, i);

      if (match (option))
        return g_object_ref (option);
    }

  return NULL;
}

static FoundryAcpPermissionResponse *
response_for_option (FoundryAcpPermissionOption *option)
{
  g_autofree char *option_id = NULL;

  g_assert (FOUNDRY_IS_ACP_PERMISSION_OPTION (option));

  option_id = foundry_acp_permission_option_dup_id (option);

  return foundry_acp_permission_response_new_selected (option_id);
}

/**
 * foundry_acp_permission_policy_new:
 * @context: (nullable): a [class@Foundry.Context]
 *
 * Creates a reusable headless permission policy for ACP requests.
 *
 * Returns: (transfer full): a [class@Foundry.AcpPermissionPolicy]
 *
 * Since: 1.2
 */
FoundryAcpPermissionPolicy *
foundry_acp_permission_policy_new (FoundryContext *context)
{
  g_autoptr(GFile) project_directory = NULL;

  g_return_val_if_fail (context == NULL || FOUNDRY_IS_CONTEXT (context), NULL);

  if (context != NULL)
    project_directory = foundry_context_dup_project_directory (context);

  return foundry_acp_permission_policy_new_for_project_directory (project_directory);
}

/**
 * foundry_acp_permission_policy_new_for_project_directory:
 * @project_directory: (nullable): project root used for containment checks
 *
 * Creates a reusable headless permission policy for ACP requests.
 *
 * Returns: (transfer full): a [class@Foundry.AcpPermissionPolicy]
 *
 * Since: 1.2
 */
FoundryAcpPermissionPolicy *
foundry_acp_permission_policy_new_for_project_directory (GFile *project_directory)
{
  FoundryAcpPermissionPolicy *self;

  g_return_val_if_fail (project_directory == NULL || G_IS_FILE (project_directory), NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_PERMISSION_POLICY, NULL);

  if (project_directory != NULL)
    self->project_directory = g_object_ref (project_directory);

  return self;
}

/**
 * foundry_acp_permission_policy_dup_project_directory:
 * @self: a [class@Foundry.AcpPermissionPolicy]
 *
 * Returns: (transfer full) (nullable): the project directory used for
 *   containment checks
 *
 * Since: 1.2
 */
GFile *
foundry_acp_permission_policy_dup_project_directory (FoundryAcpPermissionPolicy *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_POLICY (self), NULL);

  if (self->project_directory == NULL)
    return NULL;

  return g_object_ref (self->project_directory);
}

/**
 * foundry_acp_permission_policy_contains_path:
 * @self: a [class@Foundry.AcpPermissionPolicy]
 * @path: a local path from a permission request
 *
 * Checks if @path is contained by the policy project directory. Relative paths
 * are resolved below the project directory when one is configured.
 *
 * Returns: %TRUE if @path is contained by the project directory
 *
 * Since: 1.2
 */
gboolean
foundry_acp_permission_policy_contains_path (FoundryAcpPermissionPolicy *self,
                                             const char                 *path)
{
  g_autoptr(GFile) file = NULL;
  char *relative_path;
  gboolean ret;

  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_POLICY (self), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  if (self->project_directory == NULL)
    return FALSE;

  if (g_path_is_absolute (path))
    file = g_file_new_for_path (path);
  else
    file = g_file_resolve_relative_path (self->project_directory, path);

  if (g_file_equal (self->project_directory, file))
    return TRUE;

  relative_path = g_file_get_relative_path (self->project_directory, file);
  ret = relative_path != NULL && !g_str_has_prefix (relative_path, "../");
  g_free (relative_path);

  return ret;
}

/**
 * foundry_acp_permission_policy_decide:
 * @self: a [class@Foundry.AcpPermissionPolicy]
 * @request: a [class@Foundry.AcpPermissionRequest]
 *
 * Applies the headless policy to @request.
 *
 * The policy only auto-selects a non-destructive allow option when the request
 * is low risk and any provided path remains inside the configured project
 * directory. Requests outside the project, high-risk requests, unknown-risk
 * requests, or destructive defaults are cancelled unless an explicit UI layer
 * replaces this decision.
 *
 * Returns: (transfer full): a [class@Foundry.AcpPermissionResponse]
 *
 * Since: 1.2
 */
FoundryAcpPermissionResponse *
foundry_acp_permission_policy_decide (FoundryAcpPermissionPolicy  *self,
                                      FoundryAcpPermissionRequest *request)
{
  g_autoptr(FoundryAcpPermissionOption) allow_option = NULL;
  g_autoptr(FoundryAcpPermissionOption) cancel_option = NULL;
  g_autoptr(FoundryAcpPermissionOption) default_option = NULL;
  g_autoptr(GListModel) options = NULL;
  g_autofree char *default_option_id = NULL;
  g_autofree char *path = NULL;
  g_autofree char *risk = NULL;

  g_return_val_if_fail (FOUNDRY_IS_ACP_PERMISSION_POLICY (self), NULL);
  g_return_val_if_fail (request != NULL, NULL);

  path = foundry_acp_permission_request_dup_path (request);
  risk = foundry_acp_permission_request_dup_risk_level (request);
  default_option_id = foundry_acp_permission_request_dup_default_option (request);
  options = foundry_acp_permission_request_list_options (request);
  default_option = find_option_by_id (options, default_option_id);
  cancel_option = find_option_matching (options, option_matches_cancel);

  if (path != NULL && !foundry_acp_permission_policy_contains_path (self, path))
    {
      if (cancel_option != NULL)
        return response_for_option (cancel_option);

      return foundry_acp_permission_response_new_cancelled ();
    }

  if (default_option != NULL &&
      foundry_acp_permission_option_get_destructive (default_option))
    {
      if (cancel_option != NULL)
        return response_for_option (cancel_option);

      return foundry_acp_permission_response_new_cancelled ();
    }

  if (risk_requires_prompt (risk) || !risk_is_low (risk))
    {
      if (cancel_option != NULL)
        return response_for_option (cancel_option);

      return foundry_acp_permission_response_new_cancelled ();
    }

  if (default_option != NULL)
    return response_for_option (default_option);

  allow_option = find_option_matching (options, option_matches_allow);

  if (allow_option != NULL)
    return response_for_option (allow_option);

  if (cancel_option != NULL)
    return response_for_option (cancel_option);

  return foundry_acp_permission_response_new_cancelled ();
}

/**
 * foundry_acp_permission_policy_request_permission:
 * @self: a [class@Foundry.AcpPermissionPolicy]
 * @request: a [class@Foundry.AcpPermissionRequest]
 *
 * Applies the policy to @request and wraps the response in a future for use by
 * [iface@Foundry.AcpClient] implementations.
 *
 * Returns: (transfer full): a [class@Dex.Future] resolving to a
 *   [class@Foundry.AcpPermissionResponse]
 *
 * Since: 1.2
 */
DexFuture *
foundry_acp_permission_policy_request_permission (FoundryAcpPermissionPolicy  *self,
                                                  FoundryAcpPermissionRequest *request)
{
  g_autoptr(FoundryAcpPermissionResponse) response = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_ACP_PERMISSION_POLICY (self));
  dex_return_error_if_fail (request != NULL);

  response = foundry_acp_permission_policy_decide (self, request);

  return dex_future_new_take_object (g_steal_pointer (&response));
}
