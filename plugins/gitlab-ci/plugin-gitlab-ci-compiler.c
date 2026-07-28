/* plugin-gitlab-ci-compiler.c
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "foundry-ci-provider.h"

#include "plugin-gitlab-ci-compiler-private.h"
#include "plugin-gitlab-ci-error-private.h"
#include "plugin-gitlab-ci-expression-private.h"
#include "plugin-gitlab-ci-yaml-private.h"

#define REFERENCE_MEMBER "$gitlab-reference"

typedef struct
{
  PluginGitlabCiContext *context;
  JsonNode              *root;
  JsonNode              *defaults;
  JsonNode              *top_variables;
  GHashTable            *resolved;
  GHashTable            *visiting;
} Compiler;

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
  if (node == NULL || JSON_NODE_HOLDS_NULL (node) || !JSON_NODE_HOLDS_VALUE (node))
    return NULL;

  if (json_node_get_value_type (node) == G_TYPE_STRING)
    return json_node_get_string (node);

  if (json_node_get_value_type (node) == G_TYPE_BOOLEAN)
    return json_node_get_boolean (node) ? "true" : "false";

  return NULL;
}

static gboolean
boolean (JsonNode *node,
         gboolean *value)
{
  const char *string;

  g_assert (value != NULL);

  if (node == NULL || !JSON_NODE_HOLDS_VALUE (node))
    return FALSE;

  if (json_node_get_value_type (node) == G_TYPE_BOOLEAN)
    {
      *value = json_node_get_boolean (node);
      return TRUE;
    }

  if (!(string = scalar (node)))
    return FALSE;

  if (g_ascii_strcasecmp (string, "true") == 0)
    *value = TRUE;
  else if (g_ascii_strcasecmp (string, "false") == 0)
    *value = FALSE;
  else
    return FALSE;

  return TRUE;
}

static JsonNode *
new_object (void)
{
  g_autoptr(JsonObject) object = json_object_new ();

  return json_node_init_object (json_node_alloc (), object);
}

static gboolean
is_global_keyword (const char *name)
{
  static const char *keywords[] = {
    "after_script",
    "before_script",
    "cache",
    "default",
    "image",
    "include",
    "services",
    "stages",
    "variables",
    "workflow",
  };

  g_assert (name != NULL);

  for (guint i = 0; i < G_N_ELEMENTS (keywords); i++)
    {
      if (g_str_equal (keywords[i], name))
        return TRUE;
    }

  return FALSE;
}

static gboolean
is_secret_name (const char *name)
{
  g_autofree char *upper = NULL;

  g_assert (name != NULL);

  upper = g_ascii_strup (name, -1);

  return strstr (upper, "TOKEN") != NULL ||
         strstr (upper, "PASSWORD") != NULL ||
         strstr (upper, "SECRET") != NULL ||
         strstr (upper, "CREDENTIAL") != NULL ||
         g_str_has_suffix (upper, "_KEY");
}

static JsonNode *
lookup_reference (JsonNode  *root,
                  JsonNode  *path,
                  char     **path_string,
                  GError   **error)
{
  g_autoptr(GString) description = NULL;
  JsonArray *parts;
  JsonNode *current = root;

  g_assert (root != NULL);
  g_assert (path != NULL);
  g_assert (path_string != NULL);

  if (!JSON_NODE_HOLDS_ARRAY (path) ||
      json_array_get_length (json_node_get_array (path)) < 2)
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "!reference requires a path with at least two items");
      return NULL;
    }

  description = g_string_new (NULL);
  parts = json_node_get_array (path);

  for (guint i = 0; i < json_array_get_length (parts); i++)
    {
      const char *name = scalar (json_array_get_element (parts, i));

      if (name == NULL || !JSON_NODE_HOLDS_OBJECT (current))
        {
          g_set_error_literal (error,
                               PLUGIN_GITLAB_CI_ERROR,
                               PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                               "invalid !reference path");
          return NULL;
        }

      if (i > 0)
        g_string_append_c (description, '.');

      g_string_append (description, name);

      if (!(current = member (current, name)))
        {
          g_set_error (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_NOT_FOUND,
                       "!reference path item '%s' does not exist",
                       name);
          return NULL;
        }
    }

  *path_string = g_string_free (g_steal_pointer (&description), FALSE);

  return current;
}

static JsonNode *
resolve_references (JsonNode    *root,
                    JsonNode    *node,
                    GHashTable  *active,
                    guint        depth,
                    GError     **error)
{
  JsonNode *path;

  g_assert (root != NULL);
  g_assert (node != NULL);
  g_assert (active != NULL);

  if (depth > 32)
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_LIMIT_EXCEEDED,
                           "!reference nesting exceeds 32 levels");
      return NULL;
    }

  if ((path = member (node, REFERENCE_MEMBER)))
    {
      g_autofree char *description = NULL;
      JsonNode *target = lookup_reference (root, path, &description, error);
      JsonNode *result;

      if (target == NULL)
        return NULL;

      if (g_hash_table_contains (active, description))
        {
          g_set_error (error,
                       PLUGIN_GITLAB_CI_ERROR,
                       PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                       "cyclic !reference through '%s'",
                       description);
          return NULL;
        }

      g_hash_table_add (active, description);
      result = resolve_references (root, target, active, depth + 1, error);
      g_hash_table_remove (active, description);

      return result;
    }

  if (JSON_NODE_HOLDS_ARRAY (node))
    {
      g_autoptr(JsonArray) result = json_array_new ();
      JsonArray *array = json_node_get_array (node);

      for (guint i = 0; i < json_array_get_length (array); i++)
        {
          JsonNode *child = resolve_references (root,
                                                json_array_get_element (array, i),
                                                active,
                                                depth + 1,
                                                error);

          if (child == NULL)
            return NULL;

          json_array_add_element (result, child);
        }

      return json_node_init_array (json_node_alloc (), result);
    }

  if (node != NULL && JSON_NODE_HOLDS_OBJECT (node))
    {
      g_autoptr(JsonObject) result = json_object_new ();
      JsonObjectIter iter;
      const char *name;
      JsonNode *value;

      json_object_iter_init (&iter, json_node_get_object (node));
      while (json_object_iter_next (&iter, &name, &value))
        {
          JsonNode *child = resolve_references (root, value, active, depth + 1, error);

          if (child == NULL)
            return NULL;

          json_object_set_member (result, name, child);
        }

      return json_node_init_object (json_node_alloc (), result);
    }

  return json_node_copy (node);
}

static gboolean
inherit_default (JsonNode   *job,
                 const char *keyword)
{
  JsonNode *inherit = member (job, "inherit");
  JsonNode *defaults;
  gboolean value;

  g_assert (job != NULL);
  g_assert (keyword != NULL);

  if (inherit == NULL)
    return TRUE;

  if (boolean (inherit, &value))
    return value;

  if (!JSON_NODE_HOLDS_OBJECT (inherit))
    return TRUE;

  defaults = member (inherit, "default");
  if (defaults == NULL)
    return TRUE;

  if (boolean (defaults, &value))
    return value;

  if (JSON_NODE_HOLDS_ARRAY (defaults))
    {
      JsonArray *array = json_node_get_array (defaults);

      for (guint i = 0; i < json_array_get_length (array); i++)
        {
          if (g_strcmp0 (scalar (json_array_get_element (array, i)), keyword) == 0)
            return TRUE;
        }

      return FALSE;
    }

  return TRUE;
}

static gboolean
inherit_variables (JsonNode *job)
{
  JsonNode *inherit = member (job, "inherit");
  JsonNode *variables;
  gboolean value;

  g_assert (job != NULL);

  if (inherit == NULL)
    return TRUE;

  if (boolean (inherit, &value))
    return value;

  if (!JSON_NODE_HOLDS_OBJECT (inherit))
    return TRUE;

  variables = member (inherit, "variables");

  return !boolean (variables, &value) || value;
}

static JsonNode *
apply_defaults (Compiler *compiler,
                JsonNode *job)
{
  g_autoptr(JsonNode) result = NULL;
  JsonObjectIter iter;
  const char *name;
  JsonNode *value;

  g_assert (compiler != NULL);
  g_assert (job != NULL);

  result = json_node_copy (job);
  if (compiler->defaults == NULL || !JSON_NODE_HOLDS_OBJECT (compiler->defaults))
    return g_steal_pointer (&result);

  json_object_iter_init (&iter, json_node_get_object (compiler->defaults));
  while (json_object_iter_next (&iter, &name, &value))
    {
      if (member (result, name) == NULL && inherit_default (job, name))
        json_object_set_member (json_node_get_object (result), name, json_node_copy (value));
    }

  return g_steal_pointer (&result);
}

static void
remove_null_values (JsonNode *node)
{
  g_autoptr(GList) names = NULL;
  JsonObject *object;

  g_assert (node != NULL);
  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  object = json_node_get_object (node);
  names = json_object_get_members (object);

  for (const GList *iter = names; iter != NULL; iter = iter->next)
    {
      const char *name = iter->data;
      JsonNode *value = json_object_get_member (object, name);

      if (JSON_NODE_HOLDS_NULL (value))
        json_object_remove_member (object, name);
      else if (JSON_NODE_HOLDS_OBJECT (value))
        remove_null_values (value);
    }
}

static JsonNode *
resolve_job (Compiler    *compiler,
             const char  *name,
             GError     **error)
{
  g_autoptr(JsonNode) own = NULL;
  g_autoptr(JsonNode) merged = NULL;
  g_autoptr(JsonNode) next = NULL;
  JsonNode *cached;
  JsonNode *raw;
  JsonNode *extends;

  g_assert (compiler != NULL);
  g_assert (name != NULL);

  if ((cached = g_hash_table_lookup (compiler->resolved, name)))
    return json_node_ref (cached);

  if (g_hash_table_contains (compiler->visiting, name))
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                   "extends cycle contains '%s'",
                   name);
      return NULL;
    }

  if (!(raw = member (compiler->root, name)) || !JSON_NODE_HOLDS_OBJECT (raw))
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_NOT_FOUND,
                   "job '%s' extends an unknown template",
                   name);
      return NULL;
    }

  g_hash_table_add (compiler->visiting, (gpointer)name);
  own = apply_defaults (compiler, raw);
  extends = member (own, "extends");
  merged = new_object ();

  if (extends != NULL)
    {
      JsonArray *parents = JSON_NODE_HOLDS_ARRAY (extends)
                             ? json_node_get_array (extends)
                             : NULL;
      guint length = parents ? json_array_get_length (parents) : 1;

      for (guint i = 0; i < length; i++)
        {
          JsonNode *parent_node = parents ? json_array_get_element (parents, i) : extends;
          const char *parent_name = scalar (parent_node);
          g_autoptr(JsonNode) parent = NULL;

          if (parent_name == NULL)
            {
              g_set_error_literal (error,
                                   PLUGIN_GITLAB_CI_ERROR,
                                   PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                                   "extends must contain job names");
              goto failure;
            }

          if (!(parent = resolve_job (compiler, parent_name, error)))
            goto failure;

          next = plugin_gitlab_ci_json_merge (merged, parent);
          json_node_unref (g_steal_pointer (&merged));
          merged = g_steal_pointer (&next);
        }
    }

  json_object_remove_member (json_node_get_object (own), "extends");

  next = plugin_gitlab_ci_json_merge (merged, own);
  json_node_unref (g_steal_pointer (&merged));
  merged = g_steal_pointer (&next);

  remove_null_values (merged);

  g_hash_table_remove (compiler->visiting, name);
  g_hash_table_insert (compiler->resolved, g_strdup (name), json_node_ref (merged));

  return g_steal_pointer (&merged);

failure:
  g_hash_table_remove (compiler->visiting, name);

  return NULL;
}

static void
append_scalars (JsonNode  *node,
                GPtrArray *target,
                guint      depth)
{
  const char *value;

  g_assert (target != NULL);

  if (node == NULL || JSON_NODE_HOLDS_NULL (node) || depth > 16)
    return;

  if ((value = scalar (node)))
    {
      g_ptr_array_add (target, g_strdup (value));
      return;
    }

  if (JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *array = json_node_get_array (node);

      for (guint i = 0; i < json_array_get_length (array); i++)
        append_scalars (json_array_get_element (array, i), target, depth + 1);
    }
}

static gboolean
matches_paths (PluginGitlabCiContext *context,
               JsonNode              *node,
               gboolean               changes)
{
  const char *path;

  g_assert (context != NULL);

  if (node == NULL)
    return TRUE;

  if (JSON_NODE_HOLDS_OBJECT (node))
    return matches_paths (context, member (node, "paths"), changes);

  if ((path = scalar (node)))
    return changes ? plugin_gitlab_ci_context_file_changed (context, path)
                   : plugin_gitlab_ci_context_file_exists (context, path);

  if (JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *array = json_node_get_array (node);

      for (guint i = 0; i < json_array_get_length (array); i++)
        {
          if (matches_paths (context, json_array_get_element (array, i), changes))
            return TRUE;
        }
    }

  return FALSE;
}

static gboolean
rule_matches (Compiler  *compiler,
              JsonNode  *rule,
              gboolean  *matches,
              GError   **error)
{
  JsonNode *condition;
  gboolean expression = TRUE;

  g_assert (compiler != NULL);
  g_assert (rule != NULL);
  g_assert (matches != NULL);

  if (!JSON_NODE_HOLDS_OBJECT (rule))
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "rule must be a mapping");
      return FALSE;
    }

  condition = member (rule, "if");
  if (condition != NULL)
    {
      const char *text = scalar (condition);

      if (text == NULL)
        {
          g_set_error_literal (error,
                               PLUGIN_GITLAB_CI_ERROR,
                               PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                               "rule condition must be a string");
          return FALSE;
        }

      if (!plugin_gitlab_ci_expression_evaluate (text, compiler->context, &expression, error))
        return FALSE;
    }

  *matches = expression &&
             matches_paths (compiler->context, member (rule, "exists"), FALSE) &&
             matches_paths (compiler->context, member (rule, "changes"), TRUE);

  return TRUE;
}

static void
flatten_rules (JsonNode  *node,
               GPtrArray *rules,
               guint      depth)
{
  g_assert (node != NULL);
  g_assert (rules != NULL);

  if (depth > 16)
    return;

  if (JSON_NODE_HOLDS_OBJECT (node))
    {
      g_ptr_array_add (rules, node);
    }
  else if (JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *array = json_node_get_array (node);

      for (guint i = 0; i < json_array_get_length (array); i++)
        flatten_rules (json_array_get_element (array, i), rules, depth + 1);
    }
}

static gboolean
evaluate_workflow (Compiler                *compiler,
                   PluginGitlabCiPipeline  *pipeline,
                   GError                 **error)
{
  g_autoptr(GPtrArray) flattened = NULL;
  JsonNode *workflow = member (compiler->root, "workflow");
  JsonNode *rules;

  g_assert (compiler != NULL);
  g_assert (pipeline != NULL);

  if (workflow == NULL)
    return TRUE;

  if (!JSON_NODE_HOLDS_OBJECT (workflow) ||
      !(rules = member (workflow, "rules")) ||
      !JSON_NODE_HOLDS_ARRAY (rules))
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "workflow:rules must be a sequence");
      return FALSE;
    }

  pipeline->workflow_selected = FALSE;
  g_set_str (&pipeline->workflow_reason, "no workflow rule matched");
  flattened = g_ptr_array_new ();
  flatten_rules (rules, flattened, 0);

  for (guint i = 0; i < flattened->len; i++)
    {
      JsonNode *rule = g_ptr_array_index (flattened, i);
      gboolean matches;

      if (!rule_matches (compiler, rule, &matches, error))
        return FALSE;

      if (matches)
        {
          pipeline->workflow_selected = g_strcmp0 (scalar (member (rule, "when")), "never") != 0;
          g_free (pipeline->workflow_reason);
          pipeline->workflow_reason = g_strdup_printf ("workflow rule %u matched%s",
                                                       i + 1,
                                                       pipeline->workflow_selected ? "" : " with when:never");
          break;
        }
    }

  return TRUE;
}

static const char *
variable_value (JsonNode *node,
                gboolean *file)
{
  JsonNode *value;
  gboolean is_file;

  g_assert (file != NULL);

  *file = FALSE;

  if (node == NULL || JSON_NODE_HOLDS_NULL (node))
    return "";

  if (scalar (node) != NULL)
    return scalar (node);

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return NULL;

  value = member (node, "value");
  if (boolean (member (node, "file"), &is_file))
    *file = is_file;

  return value == NULL || JSON_NODE_HOLDS_NULL (value) ? "" : scalar (value);
}

static void
add_variables (GHashTable *target,
               JsonNode   *mapping)
{
  JsonObjectIter iter;
  const char *name;
  JsonNode *node;

  g_assert (target != NULL);

  if (mapping == NULL || !JSON_NODE_HOLDS_OBJECT (mapping))
    return;

  json_object_iter_init (&iter, json_node_get_object (mapping));
  while (json_object_iter_next (&iter, &name, &node))
    {
      const char *value;
      gboolean file;

      if ((value = variable_value (node, &file)))
        g_hash_table_replace (target,
                              g_strdup (name),
                              plugin_gitlab_ci_variable_new (name, value, is_secret_name (name), file));
    }
}

static void
add_context_variables (GHashTable            *target,
                       PluginGitlabCiContext *context)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_assert (target != NULL);
  g_assert (context != NULL);

  g_hash_table_iter_init (&iter, context->variables);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_replace (target,
                          g_strdup (key),
                          plugin_gitlab_ci_variable_new (key,
                                                         value,
                                                         is_secret_name (key),
                                                         FALSE));
}

static void
push_variables (PluginGitlabCiContext *context,
                GHashTable            *variables,
                GHashTable            *saved)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_assert (context != NULL);
  g_assert (variables != NULL);
  g_assert (saved != NULL);

  g_hash_table_iter_init (&iter, variables);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      PluginGitlabCiVariable *variable = value;
      const char *previous = g_hash_table_lookup (context->variables, key);

      g_hash_table_insert (saved,
                           g_strdup (key),
                           previous ? g_strdup (previous) : NULL);
      g_hash_table_replace (context->variables,
                            g_strdup (key),
                            g_strdup (variable->value));
    }
}

static void
pop_variables (PluginGitlabCiContext *context,
               GHashTable            *saved)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_assert (context != NULL);
  g_assert (saved != NULL);

  g_hash_table_iter_init (&iter, saved);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (value != NULL)
        g_hash_table_replace (context->variables, g_strdup (key), g_strdup (value));
      else
        g_hash_table_remove (context->variables, key);
    }
}

static void
replace_needs (PluginGitlabCiJob *job,
               JsonNode          *needs)
{
  JsonArray *array;
  guint len;

  g_assert (job != NULL);

  g_ptr_array_set_size (job->needs, 0);
  job->has_explicit_needs = TRUE;
  if (needs == NULL || JSON_NODE_HOLDS_NULL (needs) || !JSON_NODE_HOLDS_ARRAY (needs))
    return;

  array = json_node_get_array (needs);
  len = json_array_get_length (array);

  for (guint i = 0; i < len; i++)
    {
      JsonNode *item = json_array_get_element (array, i);
      const char *name = scalar (item);

      if (name != NULL)
        {
          g_ptr_array_add (job->needs, plugin_gitlab_ci_need_new (name, TRUE));
        }
      else if (JSON_NODE_HOLDS_OBJECT (item) &&
               (name = scalar (member (item, "job"))))
        {
          gboolean artifacts = TRUE;

          boolean (member (item, "artifacts"), &artifacts);
          g_ptr_array_add (job->needs,
                           plugin_gitlab_ci_need_new (name, artifacts));
        }
    }
}

static gboolean
evaluate_job_rules (Compiler           *compiler,
                    PluginGitlabCiJob  *job,
                    JsonNode           *definition,
                    GError            **error)
{
  g_autoptr(GHashTable) saved = NULL;
  g_autoptr(GPtrArray) flattened = NULL;
  JsonNode *rules = member (definition, "rules");

  g_assert (compiler != NULL);
  g_assert (job != NULL);
  g_assert (definition != NULL);

  if (rules == NULL)
    {
      const char *when = scalar (member (definition, "when"));

      job->when = g_strdup (when != NULL ? when : "on_success");
      if (g_str_equal (job->when, "never"))
        {
          job->status = PLUGIN_GITLAB_CI_JOB_STATUS_SKIPPED;
          job->reason = g_strdup ("job when is never");
        }
      else if (g_str_equal (job->when, "manual"))
        {
          job->status = PLUGIN_GITLAB_CI_JOB_STATUS_MANUAL;
          job->reason = g_strdup ("job is manual");
        }
      else
        {
          job->status = PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED;
          job->reason = g_strdup ("selected by default");
        }

      return TRUE;
    }

  if (!JSON_NODE_HOLDS_ARRAY (rules))
    {
      g_set_error (error,
                   PLUGIN_GITLAB_CI_ERROR,
                   PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                   "job '%s' rules must be a sequence",
                   job->name);
      return FALSE;
    }

  saved = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  push_variables (compiler->context, job->variables, saved);
  flattened = g_ptr_array_new ();
  flatten_rules (rules, flattened, 0);
  job->status = PLUGIN_GITLAB_CI_JOB_STATUS_SKIPPED;
  job->when = g_strdup ("never");
  job->reason = g_strdup ("no job rule matched");

  for (guint i = 0; i < flattened->len; i++)
    {
      JsonNode *rule = g_ptr_array_index (flattened, i);
      gboolean matches;
      gboolean allow_failure;
      const char *when;

      if (!rule_matches (compiler, rule, &matches, error))
        {
          pop_variables (compiler->context, saved);
          return FALSE;
        }

      if (!matches)
        continue;

      when = scalar (member (rule, "when"));
      g_set_str (&job->when, when ? when : "on_success");

      if (g_str_equal (job->when, "never"))
        job->status = PLUGIN_GITLAB_CI_JOB_STATUS_SKIPPED;
      else if (g_str_equal (job->when, "manual"))
        job->status = PLUGIN_GITLAB_CI_JOB_STATUS_MANUAL;
      else
        job->status = PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED;

      if (boolean (member (rule, "allow_failure"), &allow_failure))
        job->allow_failure = allow_failure;

      add_variables (job->variables, member (rule, "variables"));

      if (member (rule, "needs") != NULL)
        replace_needs (job, member (rule, "needs"));

      g_free (job->reason);
      job->reason = g_strdup_printf ("job rule %u matched%s",
                                     i + 1,
                                     g_str_equal (job->when, "never") ? " with when:never" : "");
      break;
    }

  pop_variables (compiler->context, saved);
  return TRUE;
}

static void
normalize_sequence (JsonNode  *node,
                    GPtrArray *target)
{
  const char *value;

  g_assert (target != NULL);

  if (node == NULL || JSON_NODE_HOLDS_NULL (node))
    return;

  if ((value = scalar (node)))
    {
      g_ptr_array_add (target, g_strdup (value));
    }
  else if (JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *array = json_node_get_array (node);

      for (guint i = 0; i < json_array_get_length (array); i++)
        {
          if ((value = scalar (json_array_get_element (array, i))))
            g_ptr_array_add (target, g_strdup (value));
        }
    }
}

static void
normalize_image (JsonNode          *node,
                 PluginGitlabCiJob *job)
{
  JsonNode *entrypoint;
  const char *value;

  g_assert (job != NULL);

  if (node == NULL || JSON_NODE_HOLDS_NULL (node))
    return;
  if ((value = scalar (node)))
    {
      job->image = g_strdup (value);
      return;
    }
  if (!JSON_NODE_HOLDS_OBJECT (node))
    return;

  job->image = g_strdup (scalar (member (node, "name")));
  job->image_user = g_strdup (scalar (member (member (node, "docker"), "user")));
  entrypoint = member (node, "entrypoint");

  if ((value = scalar (entrypoint)) && value[0] != '\0')
    {
      g_ptr_array_add (job->unsupported,
                       g_strdup ("custom image entrypoints are not supported"));
    }
  else if (entrypoint != NULL && JSON_NODE_HOLDS_ARRAY (entrypoint))
    {
      JsonArray *array = json_node_get_array (entrypoint);

      for (guint i = 0; i < json_array_get_length (array); i++)
        {
          value = scalar (json_array_get_element (array, i));

          if (value && value[0] != '\0')
            {
              g_ptr_array_add (job->unsupported,
                               g_strdup ("custom image entrypoints are not supported"));
              break;
            }
        }
    }
}

static int
normalize_retry (JsonNode *node)
{
  const char *string;
  char *endptr = NULL;
  long value;

  if (node != NULL && JSON_NODE_HOLDS_OBJECT (node))
    node = member (node, "max");

  if (!(string = scalar (node)))
    return 0;

  value = strtol (string, &endptr, 10);

  return endptr != string && *endptr == '\0' ? CLAMP (value, 0, 2) : 0;
}

static gboolean
is_known_job_keyword (const char *name)
{
  static const char *keywords[] = {
    "after_script",
    "allow_failure",
    "artifacts",
    "before_script",
    "cache",
    "coverage",
    "dependencies",
    "environment",
    "extends",
    "id_tokens",
    "image",
    "inherit",
    "interruptible",
    "needs",
    "release",
    "resource_group",
    "retry",
    "rules",
    "script",
    "services",
    "stage",
    "tags",
    "timeout",
    "trigger",
    "variables",
    "when",
  };

  g_assert (name != NULL);

  for (guint i = 0; i < G_N_ELEMENTS (keywords); i++)
    {
      if (g_str_equal (keywords[i], name))
        return TRUE;
    }

  return FALSE;
}

static void
classify_unsupported (PluginGitlabCiJob *job,
                      JsonNode          *definition)
{
  JsonObjectIter iter;
  const char *name;
  JsonNode *value;

  g_assert (job != NULL);
  g_assert (definition != NULL);

  json_object_iter_init (&iter, json_node_get_object (definition));
  while (json_object_iter_next (&iter, &name, &value))
    {
      if (!is_known_job_keyword (name) ||
          g_str_equal (name, "cache") ||
          g_str_equal (name, "environment") ||
          g_str_equal (name, "id_tokens") ||
          g_str_equal (name, "release") ||
          g_str_equal (name, "resource_group") ||
          g_str_equal (name, "services") ||
          g_str_equal (name, "trigger"))
        g_ptr_array_add (job->unsupported,
                         g_strdup_printf ("unsupported execution key '%s'", name));
    }

  if (job->image == NULL || job->image[0] == '\0')
    g_ptr_array_add (job->unsupported,
                     g_strdup ("jobs without an OCI image are not supported"));

  if (job->unsupported->len > 0 && job->status == PLUGIN_GITLAB_CI_JOB_STATUS_SELECTED)
    {
      job->status = PLUGIN_GITLAB_CI_JOB_STATUS_UNSUPPORTED;
      g_set_str (&job->reason, g_ptr_array_index (job->unsupported, 0));
    }
}

static void
normalize_artifacts (PluginGitlabCiJob *job,
                     JsonNode          *artifacts)
{
  JsonNode *reports;
  const char *when;

  g_assert (job != NULL);

  job->artifacts_when = g_strdup ("on_success");
  if (artifacts == NULL || !JSON_NODE_HOLDS_OBJECT (artifacts))
    return;

  normalize_sequence (member (artifacts, "paths"), job->artifact_paths);

  if ((when = scalar (member (artifacts, "when"))))
    g_set_str (&job->artifacts_when, when);

  reports = member (artifacts, "reports");

  if (reports != NULL && JSON_NODE_HOLDS_OBJECT (reports))
    {
      JsonObjectIter iter;
      const char *type;
      JsonNode *report;

      json_object_iter_init (&iter, json_node_get_object (reports));
      while (json_object_iter_next (&iter, &type, &report))
        {
          if (g_str_equal (type, "coverage_report") && JSON_NODE_HOLDS_OBJECT (report))
            {
              const char *path = scalar (member (report, "path"));

              if (path != NULL)
                g_ptr_array_add (job->artifact_paths, g_strdup (path));
            }
          else if ((g_str_equal (type, "junit") || g_str_equal (type, "codequality")) &&
                   scalar (report) != NULL)
            g_ptr_array_add (job->artifact_paths, g_strdup (scalar (report)));
          else if (g_str_equal (type, "junit") && JSON_NODE_HOLDS_ARRAY (report))
            normalize_sequence (report, job->artifact_paths);
        }
    }
}

static PluginGitlabCiJob *
normalize_job (Compiler                *compiler,
               PluginGitlabCiPipeline  *pipeline,
               const char              *name,
               JsonNode                *definition,
               GError                 **error)
{
  g_autoptr(PluginGitlabCiJob) job = NULL;
  const char *value;
  gboolean flag;

  g_assert (compiler != NULL);
  g_assert (pipeline != NULL);
  g_assert (name != NULL);
  g_assert (definition != NULL);

  job = plugin_gitlab_ci_job_new (pipeline->provider,
                                  FOUNDRY_CI_PIPELINE (pipeline));
  job->name = g_strdup (name);

  value = scalar (member (definition, "stage"));
  job->stage = g_strdup (value ? value : "test");
  normalize_image (member (definition, "image"), job);
  append_scalars (member (definition, "before_script"), job->before_script, 0);
  append_scalars (member (definition, "script"), job->script, 0);
  append_scalars (member (definition, "after_script"), job->after_script, 0);
  normalize_sequence (member (definition, "dependencies"), job->dependencies);
  if (member (definition, "needs"))
    replace_needs (job, member (definition, "needs"));
  normalize_artifacts (job, member (definition, "artifacts"));
  if (boolean (member (definition, "allow_failure"), &flag))
    job->allow_failure = flag;
  job->retry = normalize_retry (member (definition, "retry"));
  value = scalar (member (definition, "timeout"));
  job->timeout = g_strdup (value ? value : "");

  add_context_variables (job->variables, compiler->context);
  if (inherit_variables (definition))
    add_variables (job->variables, compiler->top_variables);
  add_variables (job->variables, member (definition, "variables"));
  g_hash_table_replace (job->variables,
                        g_strdup ("CI_JOB_NAME"),
                        plugin_gitlab_ci_variable_new ("CI_JOB_NAME", job->name, FALSE, FALSE));
  g_hash_table_replace (job->variables,
                        g_strdup ("CI_JOB_STAGE"),
                        plugin_gitlab_ci_variable_new ("CI_JOB_STAGE", job->stage, FALSE, FALSE));

  if (!evaluate_job_rules (compiler, job, definition, error))
    return NULL;

  if (!pipeline->workflow_selected)
    {
      job->status = PLUGIN_GITLAB_CI_JOB_STATUS_SKIPPED;
      g_set_str (&job->reason, pipeline->workflow_reason);
    }

  classify_unsupported (job, definition);
  return g_steal_pointer (&job);
}

static void
add_pipeline_variables_to_context (Compiler *compiler)
{
  JsonObjectIter iter;
  const char *name;
  JsonNode *node;

  g_assert (compiler != NULL);

  if (compiler->top_variables == NULL ||
      !JSON_NODE_HOLDS_OBJECT (compiler->top_variables))
    return;

  json_object_iter_init (&iter, json_node_get_object (compiler->top_variables));
  while (json_object_iter_next (&iter, &name, &node))
    {
      const char *value;
      gboolean file;

      if ((value = variable_value (node, &file)))
        g_hash_table_replace (compiler->context->variables,
                              g_strdup (name),
                              g_strdup (value));
    }
}

static gboolean
parse_stages (PluginGitlabCiPipeline  *pipeline,
              JsonNode                *node,
              GError                 **error)
{
  static const char *defaults[] = { ".pre", "build", "test", "deploy", ".post" };
  JsonArray *array;

  g_assert (pipeline != NULL);

  if (node == NULL)
    {
      for (guint i = 0; i < G_N_ELEMENTS (defaults); i++)
        g_ptr_array_add (pipeline->stages, g_strdup (defaults[i]));
      return TRUE;
    }

  if (!JSON_NODE_HOLDS_ARRAY (node))
    {
      g_set_error_literal (error,
                           PLUGIN_GITLAB_CI_ERROR,
                           PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                           "stages must be a sequence");
      return FALSE;
    }

  g_ptr_array_add (pipeline->stages, g_strdup (".pre"));

  array = json_node_get_array (node);

  for (guint i = 0; i < json_array_get_length (array); i++)
    {
      const char *stage = scalar (json_array_get_element (array, i));

      if (stage == NULL)
        {
          g_set_error_literal (error,
                               PLUGIN_GITLAB_CI_ERROR,
                               PLUGIN_GITLAB_CI_ERROR_INVALID_DATA,
                               "stage name must be a string");
          return FALSE;
        }

      if (!g_str_equal (stage, ".pre") && !g_str_equal (stage, ".post"))
        g_ptr_array_add (pipeline->stages, g_strdup (stage));
    }

  g_ptr_array_add (pipeline->stages, g_strdup (".post"));

  return TRUE;
}

PluginGitlabCiPipeline *
plugin_gitlab_ci_compiler_compile (FoundryCiProvider      *provider,
                                   PluginGitlabCiContext  *context,
                                   JsonNode               *config,
                                   GError                **error)
{
  Compiler compiler = {0};
  g_autoptr(GHashTable) active_references = NULL;
  g_autoptr(JsonNode) root = NULL;
  g_autoptr(PluginGitlabCiPipeline) pipeline = NULL;
  g_autofree char *configuration = NULL;
  JsonObjectIter iter;
  const char *name;
  JsonNode *raw;

  g_return_val_if_fail (FOUNDRY_IS_CI_PROVIDER (provider), NULL);
  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (config != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  active_references = g_hash_table_new (g_str_hash, g_str_equal);
  if (!(root = resolve_references (config, config, active_references, 0, error)))
    return NULL;

  compiler.context = context;
  compiler.root = root;
  compiler.defaults = member (root, "default");
  compiler.top_variables = member (root, "variables");
  compiler.resolved = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             (GDestroyNotify)json_node_unref);
  compiler.visiting = g_hash_table_new (g_str_hash, g_str_equal);
  add_pipeline_variables_to_context (&compiler);

  pipeline = plugin_gitlab_ci_pipeline_new (provider, context);

  configuration = plugin_gitlab_ci_json_format (root);
  pipeline->digest = g_compute_checksum_for_string (G_CHECKSUM_SHA256, configuration, -1);

  if (!parse_stages (pipeline, member (root, "stages"), error) ||
      !evaluate_workflow (&compiler, pipeline, error))
    goto failure;

  json_object_iter_init (&iter, json_node_get_object (root));
  while (json_object_iter_next (&iter, &name, &raw))
    {
      g_autoptr(JsonNode) definition = NULL;
      g_autoptr(PluginGitlabCiJob) job = NULL;

      if (!JSON_NODE_HOLDS_OBJECT (raw) ||
          is_global_keyword (name) ||
          g_str_has_prefix (name, "."))
        continue;

      if (!(definition = resolve_job (&compiler, name, error)))
        goto failure;

      if (!(job = normalize_job (&compiler, pipeline, name, definition, error)))
        goto failure;

      plugin_gitlab_ci_pipeline_add_job (pipeline, job);
    }

  if (!plugin_gitlab_ci_pipeline_validate (pipeline, error))
    goto failure;

  g_clear_pointer (&compiler.resolved, g_hash_table_unref);
  g_clear_pointer (&compiler.visiting, g_hash_table_unref);

  return g_steal_pointer (&pipeline);

failure:
  g_clear_pointer (&compiler.resolved, g_hash_table_unref);
  g_clear_pointer (&compiler.visiting, g_hash_table_unref);

  return NULL;
}
