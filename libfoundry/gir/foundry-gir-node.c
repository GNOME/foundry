/* foundry-gir-node.c
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

#include "foundry-gir-node.h"
#include "foundry-gir-node-private.h"

#include <ctype.h>

/**
 * FoundryGirNode:
 *
 * Represents a node in a GObject Introspection Repository (GIR) file.
 *
 * FoundryGirNode provides functionality for representing and manipulating
 * nodes in GIR files including attributes, children, and content. It
 * supports hierarchical navigation and provides efficient access to
 * GIR data for development tools and documentation generation.
 */

typedef struct _FoundryGirAttribute FoundryGirAttribute;

struct _FoundryGirNode
{
  GObject              parent_instance;
  const char          *tag_name;
  char                *name;
  GString             *content;
  FoundryGirNode      *parent;
  GQueue               attributes;
  GQueue               children;
  GList                parent_link;
  FoundryGirNodeType   type;
};

struct _FoundryGirAttribute
{
  GList       link;
  const char *key;
  char       *value;
};

G_DEFINE_FINAL_TYPE (FoundryGirNode, foundry_gir_node, G_TYPE_OBJECT)

static void
foundry_gir_node_dispose (GObject *object)
{
  FoundryGirNode *self = FOUNDRY_GIR_NODE (object);

  while (self->children.length > 0)
    {
      FoundryGirNode *child = g_queue_pop_head_link (&self->children)->data;
      child->parent = NULL;
      g_object_unref (child);
    }

  G_OBJECT_CLASS (foundry_gir_node_parent_class)->dispose (object);
}

static void
foundry_gir_node_finalize (GObject *object)
{
  FoundryGirNode *self = FOUNDRY_GIR_NODE (object);

  while (self->attributes.length > 0)
    {
      FoundryGirAttribute *attr = g_queue_pop_head_link (&self->attributes)->data;

      g_free (attr->value);
      g_free (attr);
    }

  g_clear_pointer (&self->name, g_free);

  if (self->content != NULL)
    g_string_free (g_steal_pointer (&self->content), TRUE);

  G_OBJECT_CLASS (foundry_gir_node_parent_class)->finalize (object);
}

static void
foundry_gir_node_class_init (FoundryGirNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_gir_node_dispose;
  object_class->finalize = foundry_gir_node_finalize;
}

static void
foundry_gir_node_init (FoundryGirNode *self)
{
  self->parent_link.data = self;
}

FoundryGirNode *
foundry_gir_node_new (FoundryGirNodeType  type,
                      const char         *tag_name)
{
  FoundryGirNode *self;

  g_return_val_if_fail (tag_name != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_GIR_NODE, NULL);
  self->type = type;
  self->tag_name = g_intern_string (tag_name);

  return self;
}

FoundryGirNodeType
foundry_gir_node_get_node_type (FoundryGirNode *node)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), FOUNDRY_GIR_NODE_UNKNOWN);

  return node->type;
}

const char *
foundry_gir_node_get_tag_name (FoundryGirNode *node)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  return node->tag_name;
}

const char *
foundry_gir_node_get_name (FoundryGirNode *node)
{
  const char *name = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  if (node->name != NULL)
    return node->name;

  name = foundry_gir_node_get_attribute (node, "glib:name");

  if (name == NULL)
    name = foundry_gir_node_get_attribute (node, "c:identifier");

  if (name == NULL)
    name = foundry_gir_node_get_attribute (node, "c:type");

  return name;
}

const char *
foundry_gir_node_get_content (FoundryGirNode *node)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  if (node->content == NULL || node->content->len == 0)
    return NULL;

  for (guint i = 0; i < node->content->len; i++)
    {
      if (!g_ascii_isspace (node->content->str[i]))
        return node->content->str;
    }

  return NULL;
}

const char *
foundry_gir_node_get_attribute (FoundryGirNode *node,
                                const char     *attribute)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);
  g_return_val_if_fail (attribute != NULL, NULL);

  for (const GList *iter = node->attributes.head; iter; iter = iter->next)
    {
      const FoundryGirAttribute *attr = iter->data;

      if (g_strcmp0 (attribute, attr->key) == 0)
        return attr->value;
    }

  return NULL;
}

gboolean
foundry_gir_node_has_attribute (FoundryGirNode *node,
                                const char     *attribute)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), FALSE);
  g_return_val_if_fail (attribute != NULL, FALSE);

  for (const GList *iter = node->attributes.head; iter; iter = iter->next)
    {
      const FoundryGirAttribute *attr = iter->data;

      if (g_strcmp0 (attribute, attr->key) == 0)
        return TRUE;
    }

  return FALSE;
}

/**
 * foundry_gir_node_list_attributes:
 * @node: a [class@Foundry.GirNode]
 * @n_attributes: (out) (nullable): location for number of attributes
 *
 * Gets the keys of the node.
 *
 * The caller should free the resulting array but not the elements
 * within it.
 *
 * Returns: (transfer container) (array length=n_attributes zero-terminated=1):
 */
const char **
foundry_gir_node_list_attributes (FoundryGirNode *node,
                                  guint          *n_attributes)
{
  const char **ret;
  guint i = 0;

  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  if (n_attributes != NULL)
    *n_attributes = node->attributes.length;

  ret = g_new0 (const char *, (gsize)node->attributes.length + 1);

  for (const GList *iter = node->attributes.head; iter; iter = iter->next)
    {
      const FoundryGirAttribute *attr = iter->data;

      ret[i++] = attr->key;
    }

  return ret;
}

/**
 * foundry_gir_node_get_children:
 * @node: a [class@Foundry.GirNode]
 *
 * Gets a list of children of this node in the Gir file.
 *
 * Returns: (transfer none) (nullable) (element-type FoundryGirNode):
 */
const GList *
foundry_gir_node_get_children (FoundryGirNode *node)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  return node->children.head;
}

/**
 * foundry_gir_node_find_child:
 * @node: a [class@Foundry.GirNode]
 *
 * Returns: (transfer none) (nullable):
 */
FoundryGirNode *
foundry_gir_node_find_child (FoundryGirNode     *node,
                             FoundryGirNodeType  type,
                             const char         *name)
{
  const GList *children = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  children = foundry_gir_node_get_children (node);

  for (const GList *iter = children; iter != NULL; iter = iter->next)
    {
      FoundryGirNode *child = iter->data;
      const char *child_name = NULL;

      if (type != FOUNDRY_GIR_NODE_UNKNOWN &&
          foundry_gir_node_get_node_type (child) != type)
        continue;

      if (name == NULL)
        return child;

      child_name = foundry_gir_node_get_name (child);

      if (g_strcmp0 (child_name, name) == 0)
        return child;
    }

  return NULL;
}

static void
foundry_gir_node_foreach_typed (FoundryGirNode     *node,
                                FoundryGirNodeType  type,
                                GFunc               callback,
                                gpointer            data)
{
  g_assert (FOUNDRY_IS_GIR_NODE (node));

  for (const GList *iter = node->children.head; iter; iter = iter->next)
    {
      FoundryGirNode *child = iter->data;

      if (child->type == type)
        callback (child, data);
    }
}

const char *
foundry_gir_node_type_to_string (FoundryGirNodeType type)
{
  switch (type)
    {
    case FOUNDRY_GIR_NODE_UNKNOWN:
      return "unknown";
    case FOUNDRY_GIR_NODE_REPOSITORY:
      return "repository";
    case FOUNDRY_GIR_NODE_INCLUDE:
      return "include";
    case FOUNDRY_GIR_NODE_C_INCLUDE:
      return "c:include";
    case FOUNDRY_GIR_NODE_PACKAGE:
      return "package";
    case FOUNDRY_GIR_NODE_NAMESPACE:
      return "namespace";
    case FOUNDRY_GIR_NODE_ALIAS:
      return "alias";
    case FOUNDRY_GIR_NODE_ARRAY:
      return "array";
    case FOUNDRY_GIR_NODE_BITFIELD:
      return "bitfield";
    case FOUNDRY_GIR_NODE_CALLBACK:
      return "callback";
    case FOUNDRY_GIR_NODE_CLASS:
      return "class";
    case FOUNDRY_GIR_NODE_CLASS_METHOD:
      return "class-method";
    case FOUNDRY_GIR_NODE_CLASS_VIRTUAL_METHOD:
      return "class-virtual-method";
    case FOUNDRY_GIR_NODE_CLASS_PROPERTY:
      return "class-property";
    case FOUNDRY_GIR_NODE_CONSTRUCTOR:
      return "constructor";
    case FOUNDRY_GIR_NODE_CONSTANT:
      return "constant";
    case FOUNDRY_GIR_NODE_DOC:
      return "doc";
    case FOUNDRY_GIR_NODE_DOC_PARA:
      return "doc:para";
    case FOUNDRY_GIR_NODE_DOC_TEXT:
      return "doc:text";
    case FOUNDRY_GIR_NODE_ENUM:
      return "enumeration";
    case FOUNDRY_GIR_NODE_ENUM_MEMBER:
      return "enum-member";
    case FOUNDRY_GIR_NODE_FIELD:
      return "field";
    case FOUNDRY_GIR_NODE_FUNCTION:
      return "function";
    case FOUNDRY_GIR_NODE_FUNCTION_MACRO:
      return "function-macro";
    case FOUNDRY_GIR_NODE_GLIB_BOXED:
      return "glib:boxed";
    case FOUNDRY_GIR_NODE_GLIB_ERROR_DOMAIN:
      return "glib:error-domain";
    case FOUNDRY_GIR_NODE_GLIB_SIGNAL:
      return "glib:signal";
    case FOUNDRY_GIR_NODE_IMPLEMENTS:
      return "implements";
    case FOUNDRY_GIR_NODE_INSTANCE_PARAMETER:
      return "instance-parameter";
    case FOUNDRY_GIR_NODE_INTERFACE:
      return "interface";
    case FOUNDRY_GIR_NODE_METHOD:
      return "method";
    case FOUNDRY_GIR_NODE_NAMESPACE_FUNCTION:
      return "namespace-function";
    case FOUNDRY_GIR_NODE_PARAMETER:
      return "parameter";
    case FOUNDRY_GIR_NODE_PARAMETERS:
      return "parameters";
    case FOUNDRY_GIR_NODE_PREREQUISITE:
      return "prerequisite";
    case FOUNDRY_GIR_NODE_PROPERTY:
      return "property";
    case FOUNDRY_GIR_NODE_RECORD:
      return "record";
    case FOUNDRY_GIR_NODE_RETURN_VALUE:
      return "return-value";
    case FOUNDRY_GIR_NODE_SOURCE_POSITION:
      return "source-position";
    case FOUNDRY_GIR_NODE_TYPE:
      return "type";
    case FOUNDRY_GIR_NODE_UNION:
      return "union";
    case FOUNDRY_GIR_NODE_VARARGS:
      return "varargs";
    case FOUNDRY_GIR_NODE_VIRTUAL_METHOD:
      return "virtual-method";
    default:
      return "unknown";
    }
}

void
_foundry_gir_node_add_attribute (FoundryGirNode *node,
                                 const char     *name,
                                 const char     *value)
{
  FoundryGirAttribute *attr;

  g_return_if_fail (FOUNDRY_IS_GIR_NODE (node));
  g_return_if_fail (name != NULL);

  if (value == NULL)
    value = "";

  attr = g_new0 (FoundryGirAttribute, 1);
  attr->link.data = attr;
  attr->key = g_intern_string (name);
  attr->value = g_strdup (value);

  g_queue_push_tail_link (&node->attributes, &attr->link);

  if (g_strcmp0 (name, "name") == 0)
    node->name = g_strdup (value);
}

void
_foundry_gir_node_add_child (FoundryGirNode *parent,
                             FoundryGirNode *child)
{
  g_return_if_fail (FOUNDRY_IS_GIR_NODE (parent));
  g_return_if_fail (FOUNDRY_IS_GIR_NODE (child));
  g_return_if_fail (child->parent == NULL);
  g_return_if_fail (child->parent_link.data == child);
  g_return_if_fail (child->parent_link.next == NULL);
  g_return_if_fail (child->parent_link.prev == NULL);

  child->parent = parent;
  child->parent_link.data = child;
  child->parent_link.next = NULL;
  child->parent_link.prev = NULL;

  g_queue_push_tail_link (&parent->children, &child->parent_link);
  g_object_ref (child);
}

void
_foundry_gir_node_append_text (FoundryGirNode *node,
                               const char     *text,
                               gsize           text_len)
{
  g_return_if_fail (FOUNDRY_IS_GIR_NODE (node));
  g_return_if_fail (text != NULL);

  if (text_len == 0)
    return;

  if (node->content == NULL)
    node->content = g_string_new_len (text, text_len);
  else
    g_string_append_len (node->content, text, text_len);
}

/**
 * foundry_gir_node_get_parent:
 * @node: a [class@Foundry.GirNode]
 *
 * Returns: (transfer none) (nullable):
 */
FoundryGirNode *
foundry_gir_node_get_parent (FoundryGirNode *node)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  return node->parent;
}

static void
collect_typed (gpointer data,
               gpointer user_data)
{
  FoundryGirNode *node = data;
  GPtrArray *ar = user_data;

  g_ptr_array_add (ar, node);
}

/**
 * foundry_gir_node_list_children_typed:
 * @node: a [class@Foundry.GirNode]
 * @type: the type of node to collect
 * @n_nodes: (out): location for number of nodes that matched
 *
 * Collects all of the children that match @type.
 *
 * The result is an array of borrowed pointers to children nodes.
 * In C that means you free the resulting pointer but not the
 * nodes within it.
 *
 * ```c
 * guint n_nodes = 0;
 * g_autofree FoundryGirNode **nodes = foundry_gir_node_list_children_typed (parent, type, &n_nodes);
 * ```
 *
 * Returns: (transfer container) (array length=n_nodes) (nullable):
 */
FoundryGirNode **
foundry_gir_node_list_children_typed (FoundryGirNode     *node,
                                      FoundryGirNodeType  type,
                                      guint              *n_nodes)
{
  g_autoptr(GPtrArray) ar = NULL;

  g_assert (FOUNDRY_IS_GIR_NODE (node));
  g_assert (n_nodes != NULL);

  *n_nodes = 0;

  ar = g_ptr_array_new ();
  foundry_gir_node_foreach_typed (node, type, collect_typed, ar);
  if (ar->len == 0)
    return NULL;

  *n_nodes = ar->len;

  g_ptr_array_add (ar, NULL);

  return (FoundryGirNode **)(gpointer)g_ptr_array_free (g_steal_pointer (&ar), FALSE);
}
