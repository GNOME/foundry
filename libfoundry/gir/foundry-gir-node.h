/* foundry-gir-node.h
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

#pragma once

#include <glib-object.h>

#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_GIR_NODE_TYPE (foundry_gir_node_type_get_type())

typedef enum _FoundryGirNodeType
{
  FOUNDRY_GIR_NODE_UNKNOWN = 0,
  FOUNDRY_GIR_NODE_REPOSITORY,
  FOUNDRY_GIR_NODE_INCLUDE,
  FOUNDRY_GIR_NODE_C_INCLUDE,
  FOUNDRY_GIR_NODE_PACKAGE,
  FOUNDRY_GIR_NODE_NAMESPACE,
  FOUNDRY_GIR_NODE_ALIAS,
  FOUNDRY_GIR_NODE_ARRAY,
  FOUNDRY_GIR_NODE_BITFIELD,
  FOUNDRY_GIR_NODE_CALLBACK,
  FOUNDRY_GIR_NODE_CLASS,
  FOUNDRY_GIR_NODE_CLASS_METHOD,
  FOUNDRY_GIR_NODE_CLASS_VIRTUAL_METHOD,
  FOUNDRY_GIR_NODE_CLASS_PROPERTY,
  FOUNDRY_GIR_NODE_CONSTRUCTOR,
  FOUNDRY_GIR_NODE_CONSTANT,
  FOUNDRY_GIR_NODE_DOC,
  FOUNDRY_GIR_NODE_DOC_PARA,
  FOUNDRY_GIR_NODE_DOC_TEXT,
  FOUNDRY_GIR_NODE_ENUM,
  FOUNDRY_GIR_NODE_ENUM_MEMBER,
  FOUNDRY_GIR_NODE_FIELD,
  FOUNDRY_GIR_NODE_FUNCTION,
  FOUNDRY_GIR_NODE_FUNCTION_MACRO,
  FOUNDRY_GIR_NODE_GLIB_BOXED,
  FOUNDRY_GIR_NODE_GLIB_ERROR_DOMAIN,
  FOUNDRY_GIR_NODE_GLIB_SIGNAL,
  FOUNDRY_GIR_NODE_IMPLEMENTS,
  FOUNDRY_GIR_NODE_INSTANCE_PARAMETER,
  FOUNDRY_GIR_NODE_INTERFACE,
  FOUNDRY_GIR_NODE_METHOD,
  FOUNDRY_GIR_NODE_NAMESPACE_FUNCTION,
  FOUNDRY_GIR_NODE_PARAMETER,
  FOUNDRY_GIR_NODE_PARAMETERS,
  FOUNDRY_GIR_NODE_PREREQUISITE,
  FOUNDRY_GIR_NODE_PROPERTY,
  FOUNDRY_GIR_NODE_RECORD,
  FOUNDRY_GIR_NODE_RETURN_VALUE,
  FOUNDRY_GIR_NODE_SOURCE_POSITION,
  FOUNDRY_GIR_NODE_TYPE,
  FOUNDRY_GIR_NODE_UNION,
  FOUNDRY_GIR_NODE_VARARGS,
  FOUNDRY_GIR_NODE_VIRTUAL_METHOD,
} FoundryGirNodeType;

#define FOUNDRY_GIR_NODE_LAST (FOUNDRY_GIR_NODE_VIRTUAL_METHOD+1)
#define FOUNDRY_TYPE_GIR_NODE (foundry_gir_node_get_type())

FOUNDRY_AVAILABLE_IN_1_1
G_DECLARE_FINAL_TYPE (FoundryGirNode, foundry_gir_node, FOUNDRY, GIR_NODE, GObject)

FOUNDRY_AVAILABLE_IN_1_1
GType                foundry_gir_node_type_get_type       (void) G_GNUC_CONST;
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode      *foundry_gir_node_new                 (FoundryGirNodeType  type,
                                                           const char         *tag_name);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNodeType   foundry_gir_node_get_node_type       (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
const char          *foundry_gir_node_get_tag_name        (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
const char          *foundry_gir_node_get_name            (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
const char          *foundry_gir_node_get_content         (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
const char          *foundry_gir_node_get_attribute       (FoundryGirNode     *node,
                                                           const char         *attribute);
FOUNDRY_AVAILABLE_IN_1_1
gboolean             foundry_gir_node_has_attribute       (FoundryGirNode     *node,
                                                           const char         *attribute);
FOUNDRY_AVAILABLE_IN_1_1
const char         **foundry_gir_node_list_attributes     (FoundryGirNode     *node,
                                                           guint              *n_attributes);
FOUNDRY_AVAILABLE_IN_1_1
const GList         *foundry_gir_node_get_children        (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode      *foundry_gir_node_get_parent          (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode      *foundry_gir_node_find_child          (FoundryGirNode     *node,
                                                           FoundryGirNodeType  type,
                                                           const char         *name);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode     **foundry_gir_node_list_children_typed (FoundryGirNode     *node,
                                                           FoundryGirNodeType  type,
                                                           guint              *n_nodes);
FOUNDRY_AVAILABLE_IN_1_1
GListModel          *foundry_gir_node_filter_typed        (FoundryGirNode     *node,
                                                           FoundryGirNodeType  type);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode      *foundry_gir_node_find_ancestor       (FoundryGirNode     *node,
                                                           FoundryGirNodeType  type);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode      *foundry_gir_node_get_first_child     (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode      *foundry_gir_node_get_last_child      (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode      *foundry_gir_node_get_next_sibling    (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
FoundryGirNode      *foundry_gir_node_find_doc            (FoundryGirNode     *node);
FOUNDRY_AVAILABLE_IN_1_1
const char          *foundry_gir_node_type_to_string      (FoundryGirNodeType  type);

G_END_DECLS
