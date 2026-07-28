/* plugin-gitlab-ci-config-loader.c
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-secret-service.h"
#include "foundry-soup.h"

#include "plugin-gitlab-ci-component-private.h"
#include "plugin-gitlab-ci-config-loader-private.h"
#include "plugin-gitlab-ci-error-private.h"
#include "plugin-gitlab-ci-yaml-private.h"

#define MAX_DEPTH 20
#define MAX_DOWNLOAD (4 * 1024 * 1024)

struct _PluginGitlabCiConfigLoader
{
  gatomicrefcount        ref_count;
  PluginGitlabCiContext *context;
  FoundrySecretService  *secrets;
  SoupSession           *session;
  GHashTable            *tokens;
  char                  *token;
  gboolean               offline;
};

typedef struct
{
  const char *host;
  const char *project;
  const char *ref;
} IncludeOrigin;

static JsonNode *load_node (PluginGitlabCiConfigLoader  *self,
                            GBytes                      *bytes,
                            const char                  *source,
                            const IncludeOrigin         *origin,
                            gboolean                     component,
                            JsonNode                    *inputs,
                            guint                        depth,
                            GHashTable                  *active,
                            GError                     **error);

static JsonNode *
member (JsonNode   *node,
        const char *name)
{
  g_assert (name != NULL);

  if (node == NULL || !JSON_NODE_HOLDS_OBJECT (node))
    return NULL;

  return json_object_get_member (json_node_get_object (node), name);
}

static const char *
scalar (JsonNode *node)
{
  if (node == NULL ||
      !JSON_NODE_HOLDS_VALUE (node) ||
      json_node_get_value_type (node) != G_TYPE_STRING)
    return NULL;

  return json_node_get_string (node);
}

static JsonNode *
new_object (void)
{
  g_autoptr(JsonObject) object = json_object_new ();

  return json_node_init_object (json_node_alloc (), object);
}

static gboolean
path_is_within_repository (PluginGitlabCiConfigLoader *self,
                           const char                 *path)
{
  g_autofree char *root = NULL;
  g_autofree char *canonical = NULL;
  gsize root_length;

  g_assert (self != NULL);
  g_assert (path != NULL);

  root = g_canonicalize_filename (self->context->repository_root, NULL);
  canonical = g_canonicalize_filename (path, NULL);
  root_length = strlen (root);
  return g_str_has_prefix (canonical, root) &&
         (canonical[root_length] == '\0' ||
          canonical[root_length] == G_DIR_SEPARATOR);
}

static GBytes *
load_local_bytes (PluginGitlabCiConfigLoader  *self,
                  const char                  *path,
                  GError                     **error)
{
  g_autoptr(GFile) file = NULL;

  g_assert (self != NULL);
  g_assert (path != NULL);

  if (!path_is_within_repository (self, path))
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_POLICY,
                   "local include '%s' escapes repository '%s'",
                   path,
                   self->context->repository_root);
      return NULL;
    }

  file = g_file_new_for_path (path);

  return dex_await_boxed (dex_file_load_contents_bytes (file), error);
}

static JsonNode *
load_local (PluginGitlabCiConfigLoader  *self,
            const char                  *path,
            guint                        depth,
            GHashTable                  *active,
            GError                     **error)
{
  g_autofree char *canonical = NULL;
  g_autoptr(GBytes) bytes = NULL;
  JsonNode *node;

  g_assert (self != NULL);
  g_assert (path != NULL);

  canonical = g_canonicalize_filename (path, self->context->repository_root);

  if (g_hash_table_contains (active, canonical))
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                   "include cycle detected at '%s'",
                   canonical);
      return NULL;
    }

  if (!(bytes = load_local_bytes (self, canonical, error)))
    return NULL;

  g_hash_table_add (active, g_strdup (canonical));
  node = load_node (self, bytes, canonical, NULL, FALSE, NULL, depth, active, error);
  g_hash_table_remove (active, canonical);

  return node;
}

static const char *
lookup_token (PluginGitlabCiConfigLoader *self,
              const char                 *host)
{
  g_autofree char *secret = NULL;
  const char *cached;

  g_assert (self != NULL);

  if (host == NULL || host[0] == '\0')
    return NULL;

  if (self->token != NULL &&
      self->context->server_host != NULL &&
      g_ascii_strcasecmp (host, self->context->server_host) == 0)
    return self->token;

  if ((cached = g_hash_table_lookup (self->tokens, host)))
    return cached[0] != '\0' ? cached : NULL;

  if (self->secrets != NULL)
    secret = dex_await_string (
      foundry_secret_service_lookup_api_key (self->secrets, host, "gitlab"),
      NULL);

  g_hash_table_insert (self->tokens,
                       g_strdup (host),
                       secret != NULL ? g_steal_pointer (&secret) : g_strdup (""));

  cached = g_hash_table_lookup (self->tokens, host);

  return *cached ? cached : NULL;
}

static GBytes *
download (PluginGitlabCiConfigLoader  *self,
          const char                  *uri,
          char                       **source,
          GError                     **error)
{
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(GBytes) bytes = NULL;
  const char *host;
  const char *token;
  guint status;

  g_assert (self != NULL);
  g_assert (uri != NULL);
  g_assert (source != NULL);

  if (self->offline)
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_NETWORK,
                   "offline mode cannot load remote include '%s'",
                   uri);
      return NULL;
    }

  if (!(message = soup_message_new ("GET", uri)))
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
                   "invalid include URI '%s'",
                   uri);
      return NULL;
    }

  soup_message_headers_append (soup_message_get_request_headers (message),
                               "Accept",
                               "application/yaml, text/yaml, */*");

  host = g_uri_get_host (soup_message_get_uri (message));
  if ((token = lookup_token (self, host)))
    soup_message_headers_append (soup_message_get_request_headers (message),
                                 "PRIVATE-TOKEN", token);

  if (!(bytes = dex_await_boxed (
          foundry_soup_session_send_and_read (self->session, message),
          error)))
    return NULL;

  status = soup_message_get_status (message);

  if (status < 200 || status >= 300)
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_NETWORK,
                   "include '%s' returned HTTP %u",
                   uri,
                   status);
      return NULL;
    }

  if (g_bytes_get_size (bytes) > MAX_DOWNLOAD)
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_LIMIT_EXCEEDED,
                   "include '%s' exceeds the download limit",
                   uri);
      return NULL;
    }

  *source = g_uri_to_string (soup_message_get_uri (message));

  return g_steal_pointer (&bytes);
}

static JsonNode *
load_remote (PluginGitlabCiConfigLoader  *self,
             const char                  *uri,
             const IncludeOrigin         *origin,
             gboolean                     component,
             JsonNode                    *inputs,
             guint                        depth,
             GHashTable                  *active,
             GError                     **error)
{
  g_autofree char *source = NULL;
  g_autoptr(GBytes) bytes = NULL;
  JsonNode *node;

  g_assert (self != NULL);
  g_assert (uri != NULL);

  if (g_hash_table_contains (active, uri))
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                   "include cycle detected at '%s'",
                   uri);
      return NULL;
    }

  if (!(bytes = download (self, uri, &source, error)))
    return NULL;

  g_hash_table_add (active, g_strdup (uri));
  node = load_node (self, bytes, source, origin, component, inputs, depth, active, error);
  g_hash_table_remove (active, uri);

  return node;
}

static char *
project_uri (const IncludeOrigin *origin,
             const char          *path)
{
  g_autofree char *project = NULL;
  g_autofree char *ref = NULL;
  g_autofree char *file = NULL;

  g_assert (origin != NULL);
  g_assert (origin->host != NULL);
  g_assert (origin->project != NULL);
  g_assert (origin->ref != NULL);
  g_assert (path != NULL);

  project = g_uri_escape_string (origin->project, "/", TRUE);
  ref = g_uri_escape_string (origin->ref, NULL, TRUE);
  file = g_uri_escape_string (path + strspn (path, "/"), "/", TRUE);

  return g_strdup_printf ("https://%s/%s/-/raw/%s/%s",
                          origin->host,
                          project,
                          ref,
                          file);
}

static JsonNode *
load_project_file (PluginGitlabCiConfigLoader  *self,
                   const IncludeOrigin         *origin,
                   const char                  *path,
                   gboolean                     component,
                   JsonNode                    *inputs,
                   guint                        depth,
                   GHashTable                  *active,
                   GError                     **error)
{
  g_autofree char *uri = NULL;

  g_assert (self != NULL);
  g_assert (origin != NULL);
  g_assert (path != NULL);

  uri = project_uri (origin, path);

  return load_remote (self, uri, origin, component, inputs, depth, active, error);
}

static gboolean
parse_component (const char  *reference,
                 char       **host,
                 char       **project,
                 char       **name,
                 char       **ref,
                 GError     **error)
{
  const char *at;
  const char *first_slash;
  const char *last_slash;

  g_assert (reference != NULL);
  g_assert (host != NULL);
  g_assert (project != NULL);
  g_assert (name != NULL);
  g_assert (ref != NULL);

  if (!(at = strrchr (reference, '@')) ||
      !(first_slash = strchr (reference, '/')) ||
      !(last_slash = g_strrstr_len (reference, at - reference, "/")) ||
      last_slash == first_slash ||
      at[1] == '\0')
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_INVALID_ARGUMENT,
                   "invalid component reference '%s'",
                   reference);
      return FALSE;
    }

  *host = g_strndup (reference, first_slash - reference);
  *project = g_strndup (first_slash + 1, last_slash - first_slash - 1);
  *name = g_strndup (last_slash + 1, at - last_slash - 1);
  *ref = g_strdup (at + 1);

  return TRUE;
}

static JsonNode *
load_project_include (PluginGitlabCiConfigLoader  *self,
                      JsonNode                    *mapping,
                      const IncludeOrigin         *origin,
                      guint                        depth,
                      GHashTable                  *active,
                      GError                     **error)
{
  g_autoptr(JsonNode) merged = NULL;
  const char *project = scalar (member (mapping, "project"));
  const char *ref = scalar (member (mapping, "ref"));
  JsonNode *file = member (mapping, "file");
  JsonNode *inputs = member (mapping, "inputs");
  IncludeOrigin project_origin;
  const char *host;

  g_assert (self != NULL);
  g_assert (mapping != NULL);
  g_assert (project != NULL);

  host = origin && origin->host ? origin->host : self->context->server_host;

  if (host == NULL || host[0] == '\0')
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "project include requires a GitLab server host");
      return NULL;
    }

  project_origin.host = host;
  project_origin.project = project;
  project_origin.ref = ref != NULL ? ref : "HEAD";
  merged = new_object ();

  if (file != NULL && JSON_NODE_HOLDS_ARRAY (file))
    {
      JsonArray *files = json_node_get_array (file);

      for (guint i = 0; i < json_array_get_length (files); i++)
        {
          const char *path = scalar (json_array_get_element (files, i));
          g_autoptr(JsonNode) loaded = NULL;
          g_autoptr(JsonNode) next = NULL;

          if (path == NULL)
            {
              g_set_error_literal (error,
                                   PLUGIN_GITLAB_CI_ERROR,
                                   PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                                   "project include files must be strings");
              return NULL;
            }

          if (!(loaded = load_project_file (self, &project_origin, path, inputs != NULL, inputs, depth + 1, active, error)))
            return NULL;

          next = plugin_gitlab_ci_json_merge (merged, loaded);
          json_node_unref (g_steal_pointer (&merged));
          merged = g_steal_pointer (&next);
        }

      return g_steal_pointer (&merged);
    }

  if (scalar (file) == NULL)
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "project include requires file");
      return NULL;
    }

  return load_project_file (self, &project_origin, scalar (file), !!inputs, inputs, depth + 1, active, error);
}

static JsonNode *
load_component_include (PluginGitlabCiConfigLoader  *self,
                        JsonNode                    *mapping,
                        guint                        depth,
                        GHashTable                  *active,
                        GError                     **error)
{
  g_autofree char *host = NULL;
  g_autofree char *project = NULL;
  g_autofree char *name = NULL;
  g_autofree char *ref = NULL;
  g_autofree char *path = NULL;
  g_autoptr(GError) local_error = NULL;
  JsonNode *inputs = member (mapping, "inputs");
  IncludeOrigin origin;
  JsonNode *node;

  g_assert (self != NULL);
  g_assert (mapping != NULL);

  if (!parse_component (scalar (member (mapping, "component")), &host, &project, &name, &ref, error))
    return NULL;

  origin.host = host;
  origin.project = project;
  origin.ref = ref;
  path = g_strdup_printf ("templates/%s.yml", name);
  node = load_project_file (self, &origin, path, TRUE, inputs, depth + 1, active, &local_error);

  if (node == NULL &&
      local_error != NULL &&
      local_error->domain == PLUGIN_GITLAB_CI_ERROR &&
      local_error->code == PLUGIN_GITLAB_CI_ERROR_NETWORK)
    {
      g_clear_error (&local_error);
      g_free (path);
      path = g_strdup_printf ("templates/%s/template.yml", name);
      node = load_project_file (self, &origin, path, TRUE, inputs, depth + 1, active, &local_error);
    }

  if (node == NULL)
    g_propagate_error (error, g_steal_pointer (&local_error));

  return node;
}

static JsonNode *
load_include_mapping (PluginGitlabCiConfigLoader  *self,
                      JsonNode                    *mapping,
                      const IncludeOrigin         *origin,
                      guint                        depth,
                      GHashTable                  *active,
                      GError                     **error)
{
  const char *value;

  g_assert (self != NULL);
  g_assert (mapping != NULL);

  if (member (mapping, "rules") != NULL)
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_UNSUPPORTED,
                           "include rules are not supported");
      return NULL;
    }

  if ((value = scalar (member (mapping, "local"))))
    {
      g_autofree char *path = NULL;

      if (origin != NULL && origin->project != NULL)
        return load_project_file (self, origin, value, FALSE, NULL, depth + 1, active, error);

      path = g_build_filename (self->context->repository_root,
                               value + strspn (value, "/"),
                               NULL);

      return load_local (self, path, depth + 1, active, error);
    }

  if ((value = scalar (member (mapping, "remote"))))
    return load_remote (self, value, NULL, FALSE, NULL, depth + 1, active, error);

  if ((value = scalar (member (mapping, "project"))))
    return load_project_include (self, mapping, origin, depth, active, error);

  if ((value = scalar (member (mapping, "component"))))
    return load_component_include (self, mapping, depth, active, error);

  g_set_error_literal (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_UNSUPPORTED,
                       "unsupported include mapping");
  return NULL;
}

static JsonNode *
load_include (PluginGitlabCiConfigLoader  *self,
              JsonNode                    *include,
              const IncludeOrigin         *origin,
              guint                        depth,
              GHashTable                  *active,
              GError                     **error)
{
  const char *path;

  g_assert (self != NULL);
  g_assert (include != NULL);

  if ((path = scalar (include)))
    {
      g_autofree char *absolute = NULL;

      if (origin != NULL && origin->project != NULL)
        return load_project_file (self, origin, path, FALSE, NULL, depth + 1, active, error);

      absolute = g_build_filename (self->context->repository_root,
                                   path + strspn (path, "/"),
                                   NULL);

      return load_local (self, absolute, depth + 1, active, error);
    }

  if (JSON_NODE_HOLDS_OBJECT (include))
    return load_include_mapping (self, include, origin, depth, active, error);

  g_set_error_literal (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                       "include must be a string or mapping");
  return NULL;
}

static JsonNode *
merge_includes (PluginGitlabCiConfigLoader  *self,
                JsonNode                    *root,
                const IncludeOrigin         *origin,
                guint                        depth,
                GHashTable                  *active,
                GError                     **error)
{
  g_autoptr(JsonNode) merged = NULL;
  g_autoptr(JsonNode) own = NULL;
  JsonNode *include;

  g_assert (self != NULL);
  g_assert (root != NULL);

  if (!JSON_NODE_HOLDS_OBJECT (root))
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "GitLab configuration must be a mapping");
      return NULL;
    }

  merged = new_object ();
  include = member (root, "include");

  if (include != NULL && JSON_NODE_HOLDS_ARRAY (include))
    {
      JsonArray *includes = json_node_get_array (include);

      for (guint i = 0; i < json_array_get_length (includes); i++)
        {
          g_autoptr(JsonNode) loaded = NULL;
          g_autoptr(JsonNode) next = NULL;

          loaded = load_include (self, json_array_get_element (includes, i), origin, depth, active, error);

          if (loaded == NULL)
            return NULL;

          next = plugin_gitlab_ci_json_merge (merged, loaded);
          json_node_unref (g_steal_pointer (&merged));
          merged = g_steal_pointer (&next);
        }
    }
  else if (include != NULL)
    {
      g_autoptr(JsonNode) loaded = load_include (self, include, origin, depth, active, error);
      g_autoptr(JsonNode) next = NULL;

      if (loaded == NULL)
        return NULL;

      next = plugin_gitlab_ci_json_merge (merged, loaded);
      json_node_unref (g_steal_pointer (&merged));
      merged = g_steal_pointer (&next);
    }

  own = json_node_copy (root);
  json_object_remove_member (json_node_get_object (own), "include");

  return plugin_gitlab_ci_json_merge (merged, own);
}

static JsonNode *
load_node (PluginGitlabCiConfigLoader  *self,
           GBytes                      *bytes,
           const char                  *source,
           const IncludeOrigin         *origin,
           gboolean                     component,
           JsonNode                    *inputs,
           guint                        depth,
           GHashTable                  *active,
           GError                     **error)
{
  g_autoptr(GPtrArray) documents = NULL;
  g_autoptr(JsonNode) expanded = NULL;
  JsonNode *root;

  g_assert (self != NULL);
  g_assert (bytes != NULL);
  g_assert (source != NULL);

  if (depth > MAX_DEPTH)
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_LIMIT_EXCEEDED,
                   "include recursion exceeds %u levels",
                   MAX_DEPTH);
      return NULL;
    }

  if (!(documents = plugin_gitlab_ci_yaml_parse (source, bytes, error)))
    return NULL;

  if (component)
    {
      if (!(expanded = plugin_gitlab_ci_component_expand (documents, inputs, error)))
        return NULL;

      root = expanded;
    }
  else if (documents->len == 1)
    {
      root = g_ptr_array_index (documents, 0);
    }
  else
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_UNSUPPORTED,
                   "'%s' must contain exactly one YAML document",
                   source);
      return NULL;
    }

  return merge_includes (self, root, origin, depth, active, error);
}

static DexFuture *
load_fiber (gpointer user_data)
{
  PluginGitlabCiConfigLoader *self = user_data;
  g_autoptr(GHashTable) active = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(JsonNode) root = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (self != NULL);

  if (!self->offline &&
      self->secrets != NULL &&
      !dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (self->secrets)), &error))
    {
      if (self->token == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_clear_error (&error);
      g_clear_object (&self->secrets);
    }

  active = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  if (!(bytes = load_local_bytes (self, self->context->configuration_path, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  g_hash_table_add (active, g_strdup (self->context->configuration_path));
  root = load_node (self, bytes, self->context->configuration_path, NULL, FALSE, NULL, 0, active, &error);

  if (root == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_boxed (JSON_TYPE_NODE, g_steal_pointer (&root));
}

PluginGitlabCiConfigLoader *
plugin_gitlab_ci_config_loader_new (FoundryContext        *foundry_context,
                                    PluginGitlabCiContext *context,
                                    gboolean               offline)
{
  PluginGitlabCiConfigLoader *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (foundry_context), NULL);
  g_return_val_if_fail (context != NULL, NULL);

  self = g_new0 (PluginGitlabCiConfigLoader, 1);
  g_atomic_ref_count_init (&self->ref_count);
  self->context = plugin_gitlab_ci_context_ref (context);
  self->secrets = foundry_context_dup_secret_service (foundry_context);
  self->session = soup_session_new_with_options ("user-agent",
                                                 "foundry/" PACKAGE_VERSION,
                                                 NULL);

  self->tokens = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  self->offline = offline;

  return self;
}

PluginGitlabCiConfigLoader *
plugin_gitlab_ci_config_loader_ref (PluginGitlabCiConfigLoader *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

void
plugin_gitlab_ci_config_loader_unref (PluginGitlabCiConfigLoader *self)
{
  if (!g_atomic_ref_count_dec (&self->ref_count))
    return;

  g_clear_pointer (&self->context, plugin_gitlab_ci_context_unref);
  g_clear_object (&self->secrets);
  g_clear_object (&self->session);
  g_clear_pointer (&self->tokens, g_hash_table_unref);
  g_clear_pointer (&self->token, g_free);
  g_free (self);
}

DexFuture *
plugin_gitlab_ci_config_loader_load (PluginGitlabCiConfigLoader *self)
{
  dex_return_error_if_fail (self != NULL);

  return dex_scheduler_spawn (NULL,
                              0,
                              load_fiber,
                              plugin_gitlab_ci_config_loader_ref (self),
                              (GDestroyNotify)plugin_gitlab_ci_config_loader_unref);
}
