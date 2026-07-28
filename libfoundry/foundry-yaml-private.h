/* foundry-yaml-private.h
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

#pragma once

#include <gio/gio.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

typedef enum
{
  FOUNDRY_YAML_PARSE_FLAGS_NONE                  = 0,
  FOUNDRY_YAML_PARSE_FLAGS_COERCE_INTEGERS       = 1 << 0,
  FOUNDRY_YAML_PARSE_FLAGS_EXTENDED_LITERALS     = 1 << 1,
  FOUNDRY_YAML_PARSE_FLAGS_MERGE_KEYS            = 1 << 2,
  FOUNDRY_YAML_PARSE_FLAGS_REJECT_DUPLICATE_KEYS = 1 << 3,
  FOUNDRY_YAML_PARSE_FLAGS_LIMIT_EXPANSION       = 1 << 4,
  FOUNDRY_YAML_PARSE_FLAGS_ALL_DOCUMENTS         = 1 << 5,
} FoundryYamlParseFlags;

/*
 * The callback owns no arguments. It may replace @node by stealing the
 * existing node and storing another full reference in its place.
 */
typedef gboolean (*FoundryYamlParseTagFunc) (const char  *tag,
                                             JsonNode   **node,
                                             gpointer     user_data,
                                             GError     **error);

GPtrArray *_foundry_yaml_parse (const char               *source,
                                GBytes                   *bytes,
                                FoundryYamlParseFlags     flags,
                                FoundryYamlParseTagFunc   tag_func,
                                gpointer                  tag_data,
                                GError                  **error);

G_END_DECLS
