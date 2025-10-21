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

#include "plugin-gitlab-forge.h"
#include "plugin-gitlab-listing.h"

struct _PluginGitlabForge
{
  FoundryForge     parent_instance;
  FoundrySettings *settings;
  SoupSession     *session;
};

G_DEFINE_FINAL_TYPE (PluginGitlabForge, plugin_gitlab_forge, FOUNDRY_TYPE_FORGE)

static DexFuture *
plugin_gitlab_forge_query_issues (FoundryForge      *forge,
                                  FoundryForgeQuery *query)
{
  PluginGitlabForge *self = (PluginGitlabForge *)forge;
  g_autofree char *path = NULL;
  gint64 project_id;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (FOUNDRY_IS_FORGE_QUERY (query));

  /* TODO: get project-id */
  project_id = 100;

  path = g_strdup_printf ("/api/v4/projects/%"G_GINT64_FORMAT"/issues", project_id);

  return plugin_gitlab_listing_new (self, SOUP_METHOD_GET, path, NULL);
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

static void
plugin_gitlab_forge_dispose (GObject *object)
{
  PluginGitlabForge *self = (PluginGitlabForge *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (plugin_gitlab_forge_parent_class)->dispose (object);
}

static void
plugin_gitlab_forge_finalize (GObject *object)
{
  PluginGitlabForge *self = (PluginGitlabForge *)object;

  g_clear_object (&self->session);

  G_OBJECT_CLASS (plugin_gitlab_forge_parent_class)->finalize (object);
}

static void
plugin_gitlab_forge_class_init (PluginGitlabForgeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryForgeClass *forge_class = FOUNDRY_FORGE_CLASS (klass);

  object_class->dispose = plugin_gitlab_forge_dispose;
  object_class->finalize = plugin_gitlab_forge_finalize;

  forge_class->load = plugin_gitlab_forge_load;
  forge_class->query_issues = plugin_gitlab_forge_query_issues;
}

static void
plugin_gitlab_forge_sign (PluginGitlabForge *self,
                          SoupMessage       *message)
{
  g_autofree char *secret = NULL;

  g_assert (PLUGIN_IS_GITLAB_FORGE (self));
  g_assert (SOUP_IS_MESSAGE (message));

  if (self->settings == NULL)
    return;

  secret = foundry_settings_get_string (self->settings, "secret");
  if (foundry_str_empty0 (secret))
    return;

  soup_message_headers_append (soup_message_get_request_headers (message),
                               "PRIVATE-TOKEN", secret);
}

static void
plugin_gitlab_forge_init (PluginGitlabForge *self)
{
  self->session = soup_session_new ();
}

static char *
plugin_gitlab_forge_dup_host (PluginGitlabForge *self)
{
  return foundry_settings_get_string (self->settings, "host");
}

static guint
plugin_gitlab_forge_get_port (PluginGitlabForge *self)
{
  return foundry_settings_get_uint (self->settings, "port");
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
 * Returns: (transfer full): a new [class@Soup.Message]
 */
SoupMessage *
plugin_gitlab_forge_create_message (PluginGitlabForge  *self,
                                    const char         *method,
                                    const char         *path,
                                    const char * const *params,
                                    ...)
{
  g_autofree char *uri_string = NULL;
  g_autofree char *host = NULL;
  g_autofree char *full_path = NULL;
  g_autoptr(GString) str = NULL;
  g_autoptr(GUri) uri = NULL;
  const char *key;
  va_list args;
  guint port;

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
  host = plugin_gitlab_forge_dup_host (self);
  port = plugin_gitlab_forge_get_port (self);

  uri = g_uri_build (G_URI_FLAGS_NONE,
                     "https",
                     NULL,
                     host,
                     port,
                     full_path,
                     str->str,
                     NULL);
  uri_string = g_uri_to_string (uri);

  return soup_message_new (method, uri_string);
}

static void
plugin_gitlab_forge_send_message_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  SoupSession *session = (SoupSession *)object;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (SOUP_IS_SESSION (session));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  if ((stream = soup_session_send_finish (session, result, &error)))
    dex_promise_resolve_object (promise, g_steal_pointer (&stream));
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
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
DexFuture *
plugin_gitlab_forge_send_message (PluginGitlabForge *self,
                                  SoupMessage       *message)
{
  DexPromise *promise;

  dex_return_error_if_fail (PLUGIN_IS_GITLAB_FORGE (self));
  dex_return_error_if_fail (SOUP_IS_MESSAGE (message));

  plugin_gitlab_forge_sign (self, message);

  promise = dex_promise_new_cancellable ();

  soup_session_send_async (self->session,
                           message,
                           G_PRIORITY_DEFAULT,
                           dex_promise_get_cancellable (promise),
                           plugin_gitlab_forge_send_message_cb,
                           dex_ref (promise));

  return DEX_FUTURE (promise);
}

static DexFuture *
plugin_gitlab_forge_load_from_string_cb (DexFuture *completed,
                                         gpointer   user_data)
{
  JsonParser *parser = user_data;

  return dex_future_new_take_boxed (JSON_TYPE_NODE,
                                    json_node_ref (json_parser_get_root (parser)));
}

static DexFuture *
plugin_gitlab_forge_send_message_and_read_json_cb (DexFuture *completed,
                                                   gpointer   user_data)
{
  g_autoptr(GInputStream) stream = dex_await_object (dex_ref (completed), NULL);
  g_autoptr(JsonParser) parser = json_parser_new ();

  return dex_future_then (foundry_json_parser_load_from_stream (parser, stream),
                          plugin_gitlab_forge_load_from_string_cb,
                          g_object_ref (parser),
                          g_object_unref);
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

  return dex_future_then (plugin_gitlab_forge_send_message (self, message),
                          plugin_gitlab_forge_send_message_and_read_json_cb,
                          NULL, NULL);
}
