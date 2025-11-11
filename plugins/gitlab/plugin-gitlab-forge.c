/* plugin-gitlab-forge.c
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
#include "plugin-gitlab-project.h"
#include "plugin-gitlab-user.h"

struct _PluginGitlabForge
{
  FoundryForge     parent_instance;
  FoundrySettings *settings;
};

G_DEFINE_FINAL_TYPE (PluginGitlabForge, plugin_gitlab_forge, FOUNDRY_TYPE_FORGE)

static SoupSession *session;

static DexFuture *
plugin_gitlab_forge_discover_path_part_fiber (gpointer user_data)
{
  PluginGitlabForge *self = user_data;
  g_autoptr(FoundryVcsManager) vcs_manager = NULL;
  g_autoptr(FoundryVcsRemote) origin = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryGitUri) git_uri = NULL;
  g_autoptr(FoundryVcs) vcs = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GString) gstr = NULL;
  g_autofree char *git_uri_str = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  vcs_manager = foundry_context_dup_vcs_manager (context);

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (vcs_manager)), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  vcs = foundry_vcs_manager_dup_vcs (vcs_manager);

  if (!FOUNDRY_IS_GIT_VCS (vcs))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Gitlab forge enabled but not a Git VCS");

  if (!(origin = dex_await_object (foundry_vcs_find_remote (vcs, "origin"), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(git_uri_str = foundry_vcs_remote_dup_uri (origin)))
    return dex_future_new_reject (FOUNDRY_FORGE_ERROR,
                                  FOUNDRY_FORGE_ERROR_NOT_CONFIGURED,
                                  "Git origin lacking url");

  if (!(git_uri = foundry_git_uri_new (git_uri_str)))
    return dex_future_new_reject (FOUNDRY_FORGE_ERROR,
                                  FOUNDRY_FORGE_ERROR_NOT_CONFIGURED,
                                  "Unsupported Git URL for origin");

  gstr = g_string_new (foundry_git_uri_get_path (git_uri));

  if (g_str_has_prefix (gstr->str, "~/"))
    g_string_erase (gstr, 0, 2);

  if (g_str_has_suffix (gstr->str, ".git"))
    g_string_truncate (gstr, gstr->len - 4);

  return dex_future_new_take_string (g_string_free (g_steal_pointer (&gstr), FALSE));
}

static DexFuture *
plugin_gitlab_forge_discover_path_part (PluginGitlabForge *self)
{
  g_assert (PLUGIN_IS_GITLAB_FORGE (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_gitlab_forge_discover_path_part_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
plugin_gitlab_forge_load (FoundryForge *forge)
{
  PluginGitlabForge *self = (PluginGitlabForge *)forge;
  g_autoptr(FoundryContext) context = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  self->settings = foundry_context_load_settings (context, "app.devsuite.foundry.gitlab", NULL);

  return dex_future_new_true ();
}

static DexFuture *
plugin_gitlab_forge_find_project_fiber (gpointer user_data)
{
  PluginGitlabForge *self = user_data;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path_part = NULL;
  g_autofree char *api_path = NULL;
  g_autofree char *escaped = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));

  if (!(path_part = dex_await_string (plugin_gitlab_forge_discover_path_part (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  escaped = g_uri_escape_string (path_part, NULL, TRUE);
  api_path = g_strdup_printf ("/api/v4/projects/%s", escaped);

  if (!(message = dex_await_object (plugin_gitlab_forge_create_message (self, SOUP_METHOD_GET, api_path, NULL, NULL), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(node = dex_await_boxed (plugin_gitlab_forge_send_message_and_read_json (self, message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (SOUP_IS_SESSION (session));
  g_assert (SOUP_IS_MESSAGE (message));
  g_assert (node != NULL);

  if (plugin_gitlab_error_extract (message, node, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (plugin_gitlab_project_new (self, g_steal_pointer (&node)));
}

static DexFuture *
plugin_gitlab_forge_find_project (FoundryForge *forge)
{
  g_assert (PLUGIN_IS_GITLAB_FORGE (forge));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_gitlab_forge_find_project_fiber,
                              g_object_ref (forge),
                              g_object_unref);
}

static DexFuture *
plugin_gitlab_forge_find_user_fiber (gpointer user_data)
{
  PluginGitlabForge *self = user_data;
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));

  if (!(message = dex_await_object (plugin_gitlab_forge_create_message (self, SOUP_METHOD_GET, "/api/v4/user", NULL, NULL), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(node = dex_await_boxed (plugin_gitlab_forge_send_message_and_read_json (self, message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (SOUP_IS_SESSION (session));
  g_assert (SOUP_IS_MESSAGE (message));
  g_assert (node != NULL);

  if (plugin_gitlab_error_extract (message, node, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (plugin_gitlab_user_new (self, g_steal_pointer (&node)));
}

static DexFuture *
plugin_gitlab_forge_find_user (FoundryForge *forge)
{
  g_assert (PLUGIN_IS_GITLAB_FORGE (forge));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_gitlab_forge_find_user_fiber,
                              g_object_ref (forge),
                              g_object_unref);
}

static void
plugin_gitlab_forge_dispose (GObject *object)
{
  PluginGitlabForge *self = (PluginGitlabForge *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (plugin_gitlab_forge_parent_class)->dispose (object);
}

static void
plugin_gitlab_forge_class_init (PluginGitlabForgeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryForgeClass *forge_class = FOUNDRY_FORGE_CLASS (klass);

  object_class->dispose = plugin_gitlab_forge_dispose;

  forge_class->load = plugin_gitlab_forge_load;
  forge_class->find_project = plugin_gitlab_forge_find_project;
  forge_class->find_user = plugin_gitlab_forge_find_user;
}

static void
plugin_gitlab_forge_init (PluginGitlabForge *self)
{
  if (session == NULL)
    session = soup_session_new ();
}

static DexFuture *
plugin_gitlab_forge_query_host_fiber (gpointer user_data)
{
  PluginGitlabForge *self = user_data;
  g_autofree char *host = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));

  host = foundry_settings_get_string (self->settings, "host");

  if (foundry_str_empty0 (host))
    {
      g_autoptr(FoundryVcsManager) vcs_manager = NULL;
      g_autoptr(FoundryVcsRemote) origin = NULL;
      g_autoptr(FoundryContext) context = NULL;
      g_autoptr(FoundryGitUri) git_uri = NULL;
      g_autoptr(FoundryVcs) vcs = NULL;
      g_autoptr(GError) error = NULL;
      g_autofree char *git_uri_str = NULL;
      g_autoptr(GUri) parsed = NULL;
      const char *git_host = NULL;

      context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
      vcs_manager = foundry_context_dup_vcs_manager (context);
      vcs = foundry_vcs_manager_dup_vcs (vcs_manager);

      if (!FOUNDRY_IS_GIT_VCS (vcs))
        return foundry_future_new_not_supported ();

      if (!(origin = dex_await_object (foundry_vcs_find_remote (vcs, "origin"), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(git_uri_str = foundry_vcs_remote_dup_uri (origin)))
        return dex_future_new_reject (FOUNDRY_FORGE_ERROR,
                                      FOUNDRY_FORGE_ERROR_NOT_CONFIGURED,
                                      "Git origin lacking url");

      if (!(git_uri = foundry_git_uri_new (git_uri_str)))
        return dex_future_new_reject (FOUNDRY_FORGE_ERROR,
                                      FOUNDRY_FORGE_ERROR_NOT_CONFIGURED,
                                      "Unsupported Git URL for origin");

      git_host = foundry_git_uri_get_host (git_uri);

      /* Work around gitlab forges like ssh.git.gnome.org */
      if (g_str_has_prefix (git_host, "ssh."))
        git_host = git_host + strlen ("ssh.");

      g_set_str (&host, git_host);
    }

  return dex_future_new_take_string (g_steal_pointer (&host));
}

static DexFuture *
plugin_gitlab_forge_query_host (PluginGitlabForge *self)
{
  g_assert (PLUGIN_IS_GITLAB_FORGE (self));

  return dex_scheduler_spawn (NULL, 0,
                              plugin_gitlab_forge_query_host_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
plugin_gitlab_forge_sign_fiber (PluginGitlabForge *self,
                                SoupMessage       *message)
{
  g_autoptr(FoundrySecretService) secrets = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GError) error = NULL;
  SoupMessageHeaders *headers;
  g_autofree char *host = NULL;
  g_autofree char *secret = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (SOUP_IS_MESSAGE (message));

  headers = soup_message_get_request_headers (message);

  if (soup_message_headers_get_one (headers, "PRIVATE-TOKEN") != NULL)
    return dex_future_new_true ();

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))) ||
      !(secrets = foundry_context_dup_secret_service (context)))
    return dex_future_new_true ();

  if (!dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (secrets)), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(host = dex_await_string (plugin_gitlab_forge_query_host (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (foundry_str_empty0 (host))
    return dex_future_new_true ();

  if ((secret = dex_await_string (foundry_secret_service_lookup_api_key (secrets, host, "gitlab"), &error)))
    {
      soup_message_headers_append (headers, "PRIVATE-TOKEN", secret);
      return dex_future_new_true ();
    }

  return dex_future_new_true ();
}

static DexFuture *
plugin_gitlab_forge_sign (PluginGitlabForge *self,
                          SoupMessage       *message)
{
  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (SOUP_IS_MESSAGE (message));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gitlab_forge_sign_fiber),
                                  2,
                                  PLUGIN_TYPE_GITLAB_FORGE, self,
                                  SOUP_TYPE_MESSAGE, message);
}

static DexFuture *
plugin_gitlab_forge_create_uri_fiber (PluginGitlabForge *self,
                                      const char        *path,
                                      const char        *query,
                                      const char        *fragment)
{
  g_autoptr(GError) error = NULL;
  g_autofree char *host = NULL;
  guint port;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (path != NULL);

  if (!(host = dex_await_string (plugin_gitlab_forge_query_host (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  port = foundry_settings_get_uint (self->settings, "port");

  return dex_future_new_take_boxed (G_TYPE_URI,
                                    g_uri_build ((G_URI_FLAGS_ENCODED_PATH | G_URI_FLAGS_ENCODED_QUERY),
                                                 "https", NULL,
                                                 host, port,
                                                 path, query, fragment));
}

static DexFuture *
plugin_gitlab_forge_create_uri (PluginGitlabForge *self,
                                const char        *path,
                                const char        *query,
                                const char        *fragment)
{
  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (path != NULL);

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gitlab_forge_create_uri_fiber),
                                  4,
                                  PLUGIN_TYPE_GITLAB_FORGE, self,
                                  G_TYPE_STRING, path,
                                  G_TYPE_STRING, query,
                                  G_TYPE_STRING, fragment);
}

static char *
plugin_gitlab_forge_create_path (PluginGitlabForge *self,
                                 const char        *path)
{
  g_autofree char *base_path = foundry_settings_get_string (self->settings, "base-path");

  if (!base_path[0] || g_str_equal (base_path, "/"))
    return g_strdup (path);

  if (!g_str_has_suffix (base_path, "/"))
    return g_strconcat (base_path, "/", path, NULL);

  return g_strconcat (base_path, path, NULL);
}

static DexFuture *
create_message_for_uri (DexFuture *completed,
                        gpointer   user_data)
{
  const char *method = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GUri) uri = NULL;
  g_autofree char *uri_string = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (method != NULL);

  if (!(uri = dex_await_boxed (dex_ref (completed), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  uri_string = g_uri_to_string (uri);

  return dex_future_new_take_object (soup_message_new (method, uri_string));
}

/**
 * plugin_gitlab_forge_create_message:
 * @self: a [class@Plugin.GitlabForge]
 * @method: the HTTP method such as "GET" (SOUP_METHOD_GET)
 * @path: the path part to use from the configured forge endpoint
 * @params: key=value params to include
 * @...: extra params to include, key followed by value with trailing NULL.
 *
 * Creates a new message that can be modified by the caller before
 * sending using plugin_gitlab_forge_send_message().
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *  a new [class@Soup.Message] or rejects with error.
 */
DexFuture *
plugin_gitlab_forge_create_message (PluginGitlabForge   *self,
                                    const char          *method,
                                    const char          *path,
                                    const char * const  *params,
                                    ...)
{
  g_autofree char *uri_string = NULL;
  g_autofree char *host = NULL;
  g_autofree char *full_path = NULL;
  g_autoptr(GString) str = NULL;
  g_autoptr(GUri) uri = NULL;
  const char *key;
  va_list args;

  g_return_val_if_fail (PLUGIN_IS_GITLAB_FORGE (self), NULL);
  g_return_val_if_fail (method != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  str = g_string_new (NULL);

  if (params != NULL)
    {
      for (guint i = 0; params[i]; i++)
        {
          const char *eq = strchr (params[i], '=');
          g_autofree char *pkey = g_strndup (params[i], eq - params[i]);
          g_autofree char *value = g_strdup (eq + 1);

          if (str->len > 0)
            g_string_append_c (str, '&');

          g_string_append_uri_escaped (str, pkey, NULL, TRUE);
          g_string_append_c (str, '=');
          g_string_append_uri_escaped (str, value, NULL, TRUE);
        }
    }

  va_start (args, params);
  key = va_arg (args, const char *);
  while (key != NULL)
    {
      const char *value = va_arg (args, const char *);

      if (str->len > 0)
        g_string_append_c (str, '&');

      g_string_append_uri_escaped (str, key, NULL, TRUE);
      g_string_append_c (str, '=');
      g_string_append_uri_escaped (str, value, NULL, TRUE);

      key = va_arg (args, const char *);
    }
  va_end (args);

  full_path = plugin_gitlab_forge_create_path (self, path);

  return dex_future_then (plugin_gitlab_forge_create_uri (self, full_path, str->str, NULL),
                          create_message_for_uri,
                          g_strdup (method),
                          g_free);
}


/**
 * plugin_gitlab_forge_send_message:
 * @self: a [class@Plugin.GitlabForge]
 * @message: the message to send
 *
 * Sends @message and completes to a [class@Gio.InputStream].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gio.InputStream].
 */
static DexFuture *
plugin_gitlab_forge_send_message_fiber (PluginGitlabForge *self,
                                        SoupMessage       *message)
{
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (SOUP_IS_MESSAGE (message));

  if (!dex_await (plugin_gitlab_forge_sign (self, message), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(stream = dex_await_object (foundry_soup_session_send (session, message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&stream));
}

DexFuture *
plugin_gitlab_forge_send_message (PluginGitlabForge *self,
                                  SoupMessage       *message)
{
  dex_return_error_if_fail (PLUGIN_IS_GITLAB_FORGE (self));
  dex_return_error_if_fail (SOUP_IS_MESSAGE (message));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gitlab_forge_send_message_fiber),
                                  2,
                                  PLUGIN_TYPE_GITLAB_FORGE, self,
                                  SOUP_TYPE_MESSAGE, message);
}

static DexFuture *
plugin_gitlab_forge_send_message_and_read_json_fiber (PluginGitlabForge *self,
                                                      SoupMessage       *message)
{
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (SOUP_IS_MESSAGE (message));

  if (!(stream = dex_await_object (plugin_gitlab_forge_send_message (self, message), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  parser = json_parser_new ();

  if (!dex_await (foundry_json_parser_load_from_stream (parser, stream), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_boxed (JSON_TYPE_NODE, json_parser_steal_root (parser));
}

/**
 * plugin_gitlab_forge_send_message_and_read_json:
 * @self: a [class@Plugin.GitlabForge]
 *
 * Like plugin_gitlab_forge_send_message() but also parses the result
 * as JSON into a [struct@Json.Node].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [struct@Json.Node] or rejects with error.
 */
DexFuture *
plugin_gitlab_forge_send_message_and_read_json (PluginGitlabForge *self,
                                                SoupMessage       *message)
{
  dex_return_error_if_fail (PLUGIN_IS_GITLAB_FORGE (self));
  dex_return_error_if_fail (SOUP_IS_MESSAGE (message));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (plugin_gitlab_forge_send_message_and_read_json_fiber),
                                  2,
                                  PLUGIN_TYPE_GITLAB_FORGE, self,
                                  SOUP_TYPE_MESSAGE, message);
}
