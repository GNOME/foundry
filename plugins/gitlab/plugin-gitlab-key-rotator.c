/* plugin-gitlab-key-rotator.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <foundry-soup.h>
#include <foundry-secret-service.h>

#include "plugin-gitlab-error.h"
#include "plugin-gitlab-forge.h"
#include "plugin-gitlab-key-rotator.h"

struct _PluginGitlabKeyRotator
{
  FoundryKeyRotator parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginGitlabKeyRotator, plugin_gitlab_key_rotator, FOUNDRY_TYPE_KEY_ROTATOR)

static DexFuture *
plugin_gitlab_key_rotator_check_expires_at_fiber (PluginGitlabKeyRotator *self,
                                                  const char             *host,
                                                  const char             *service_name,
                                                  const char             *secret)
{
  g_autoptr(FoundryForgeManager) forge_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryForge) forge = NULL;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GDateTime) expires_at = NULL;
  const char *expires_at_str = NULL;
  g_autofree char *iso8601_str = NULL;

  g_assert (PLUGIN_IS_GITLAB_KEY_ROTATOR (self));
  g_assert (host != NULL);
  g_assert (service_name != NULL);
  g_assert (secret != NULL);

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    return foundry_future_new_not_supported ();

  if (!(forge_manager = foundry_context_dup_forge_manager (context)))
    return foundry_future_new_not_supported ();

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (forge_manager)), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(forge = foundry_forge_manager_dup_forge (forge_manager)))
    return dex_future_new_reject (FOUNDRY_FORGE_ERROR,
                                  FOUNDRY_FORGE_ERROR_NOT_CONFIGURED,
                                  "No forge configured");

  if (!PLUGIN_IS_GITLAB_FORGE (forge))
    return dex_future_new_reject (FOUNDRY_FORGE_ERROR,
                                  FOUNDRY_FORGE_ERROR_NOT_CONFIGURED,
                                  "Current forge is not a GitLab forge");

  if (!(message = dex_await_object (plugin_gitlab_forge_create_message (PLUGIN_GITLAB_FORGE (forge),
                                                                        "GET",
                                                                        "/api/v4/personal_access_tokens/self",
                                                                        NULL, NULL),
                                    &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  soup_message_headers_append (soup_message_get_request_headers (message), "PRIVATE-TOKEN", secret);

  if (!(node = dex_await_boxed (plugin_gitlab_forge_send_message_and_read_json (PLUGIN_GITLAB_FORGE (forge), message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (plugin_gitlab_error_extract (message, node, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Unexpected JSON reply");

  if (!FOUNDRY_JSON_OBJECT_PARSE (node, "expires_at", FOUNDRY_JSON_NODE_GET_STRING (&expires_at_str)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Missing expires_at field in response");

  if (expires_at_str == NULL || expires_at_str[0] == '\0')
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "expires_at field is empty");

  iso8601_str = g_strconcat (expires_at_str, "T00:00:00Z", NULL);
  expires_at = g_date_time_new_from_iso8601 (iso8601_str, NULL);

  if (expires_at == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Invalid date format in expires_at field");

  return dex_future_new_take_boxed (G_TYPE_DATE_TIME, g_steal_pointer (&expires_at));
}

static DexFuture *
plugin_gitlab_key_rotator_rotate_fiber (PluginGitlabKeyRotator *self,
                                        const char             *host,
                                        const char             *service_name,
                                        const char             *secret,
                                        GDateTime              *expire_at)
{
  g_autoptr(FoundryForgeManager) forge_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryForge) forge = NULL;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(JsonNode) body_node = NULL;
  g_autoptr(GBytes) body_bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *expires_at_str = NULL;
  const char *token = NULL;

  g_assert (PLUGIN_IS_GITLAB_KEY_ROTATOR (self));
  g_assert (host != NULL);
  g_assert (service_name != NULL);
  g_assert (secret != NULL);

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    return foundry_future_new_not_supported ();

  if (!(forge_manager = foundry_context_dup_forge_manager (context)))
    return foundry_future_new_not_supported ();

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (forge_manager)), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(forge = foundry_forge_manager_dup_forge (forge_manager)))
    return dex_future_new_reject (FOUNDRY_FORGE_ERROR,
                                  FOUNDRY_FORGE_ERROR_NOT_CONFIGURED,
                                  "No forge configured");

  if (!PLUGIN_IS_GITLAB_FORGE (forge))
    return dex_future_new_reject (FOUNDRY_FORGE_ERROR,
                                  FOUNDRY_FORGE_ERROR_NOT_CONFIGURED,
                                  "Current forge is not a GitLab forge");

  if (!(message = dex_await_object (plugin_gitlab_forge_create_message (PLUGIN_GITLAB_FORGE (forge),
                                                                        "POST",
                                                                        "/api/v4/personal_access_tokens/self/rotate",
                                                                        NULL, NULL),
                                    &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  soup_message_headers_append (soup_message_get_request_headers (message), "PRIVATE-TOKEN", secret);

  if (expire_at != NULL)
    {
      expires_at_str = g_date_time_format (expire_at, "%Y-%m-%d");
      body_node = FOUNDRY_JSON_OBJECT_NEW ("expires_at", FOUNDRY_JSON_NODE_PUT_STRING (expires_at_str));

      if (!(body_bytes = dex_await_boxed (foundry_json_node_to_bytes (body_node), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      soup_message_set_request_body_from_bytes (message, "application/json", body_bytes);
    }

  if (!(node = dex_await_boxed (plugin_gitlab_forge_send_message_and_read_json (PLUGIN_GITLAB_FORGE (forge), message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (plugin_gitlab_error_extract (message, node, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Unexpected JSON reply");

  if (!FOUNDRY_JSON_OBJECT_PARSE (node, "token", FOUNDRY_JSON_NODE_GET_STRING (&token)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_DATA,
                                  "Missing token field in response");

  return dex_future_new_take_string (g_strdup (token));
}

static DexFuture *
plugin_gitlab_key_rotator_check_expires_at (FoundryKeyRotator *rotator,
                                            const char        *host,
                                            const char        *service_name,
                                            const char        *secret)
{
  PluginGitlabKeyRotator *self = PLUGIN_GITLAB_KEY_ROTATOR (rotator);

  g_assert (PLUGIN_IS_GITLAB_KEY_ROTATOR (self));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gitlab_key_rotator_check_expires_at_fiber),
                                  4,
                                  PLUGIN_TYPE_GITLAB_KEY_ROTATOR, self,
                                  G_TYPE_STRING, host,
                                  G_TYPE_STRING, service_name,
                                  G_TYPE_STRING, secret);
}

static DexFuture *
plugin_gitlab_key_rotator_rotate (FoundryKeyRotator *rotator,
                                  const char        *host,
                                  const char        *service_name,
                                  const char        *secret,
                                  GDateTime         *expire_at)
{
  PluginGitlabKeyRotator *self = PLUGIN_GITLAB_KEY_ROTATOR (rotator);

  g_assert (PLUGIN_IS_GITLAB_KEY_ROTATOR (self));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gitlab_key_rotator_rotate_fiber),
                                  5,
                                  PLUGIN_TYPE_GITLAB_KEY_ROTATOR, self,
                                  G_TYPE_STRING, host,
                                  G_TYPE_STRING, service_name,
                                  G_TYPE_STRING, secret,
                                  G_TYPE_DATE_TIME, expire_at);
}

static gboolean
plugin_gitlab_key_rotator_can_rotate (FoundryKeyRotator *rotator,
                                      const char        *host,
                                      const char        *service_name,
                                      const char        *secret)
{
  return foundry_str_equal0 (service_name, "gitlab");
}

static void
plugin_gitlab_key_rotator_class_init (PluginGitlabKeyRotatorClass *klass)
{
  FoundryKeyRotatorClass *rotator_class = FOUNDRY_KEY_ROTATOR_CLASS (klass);

  rotator_class->check_expires_at = plugin_gitlab_key_rotator_check_expires_at;
  rotator_class->rotate = plugin_gitlab_key_rotator_rotate;
  rotator_class->can_rotate = plugin_gitlab_key_rotator_can_rotate;
}

static void
plugin_gitlab_key_rotator_init (PluginGitlabKeyRotator *self)
{
}
