/* plugin-gitlab-util.c
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

#include "plugin-gitlab-util.h"

void
plugin_gitlab_query_build_params (FoundryForgeQuery *query,
                                  GStrvBuilder      *builder)
{
  g_autofree char *keywords = NULL;

  g_return_if_fail (FOUNDRY_IS_FORGE_QUERY (query));
  g_return_if_fail (builder != NULL);

  if (foundry_forge_query_contains_state (query, "all"))
    g_strv_builder_add (builder, "state=all");
  else if (foundry_forge_query_contains_state (query, "merged"))
    g_strv_builder_add (builder, "state=merged");
  else if (foundry_forge_query_contains_state (query, "open"))
    g_strv_builder_add (builder, "state=opened");
  else if (foundry_forge_query_contains_state (query, "closed"))
    g_strv_builder_add (builder, "state=closed");
  else
    g_strv_builder_add (builder, "state=opened");

  if ((keywords = foundry_forge_query_dup_keywords (query)))
    {
      g_autofree char *search = g_strdup_printf ("search=%s", keywords);
      g_autoptr(GString) scope = g_string_new ("in=");
      gboolean all = FALSE;

      g_strv_builder_add (builder, search);

      all = foundry_forge_query_contains_keywords_scope (query, "all");

      if (all || foundry_forge_query_contains_keywords_scope (query, "title"))
        g_string_append (scope, "title,");

      if (all || foundry_forge_query_contains_keywords_scope (query, "description"))
        g_string_append (scope, "description,");

      g_assert (scope->len > 0);

      if (scope->str[scope->len - 1] == ',')
        g_string_truncate (scope, scope->len - 1);

      if (!g_str_equal (scope->str, "in="))
        g_strv_builder_add (builder, scope->str);
    }
}
