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

#include <gio/gio.h>

#include "foundry-gir-node.h"
#include "foundry-gir-node-private.h"

#include <ctype.h>

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

static GType
foundry_gir_node_get_item_type (GListModel *model)
{
  return FOUNDRY_TYPE_GIR_NODE;
}

static guint
foundry_gir_node_get_n_items (GListModel *model)
{
  return FOUNDRY_GIR_NODE (model)->children.length;
}

static gpointer
foundry_gir_node_get_item (GListModel *model,
                           guint       position)
{
  FoundryGirNode *self = FOUNDRY_GIR_NODE (model);

  if (position >= self->children.length)
    return NULL;

  return g_object_ref (g_queue_peek_nth (&self->children, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_gir_node_get_item_type;
  iface->get_n_items = foundry_gir_node_get_n_items;
  iface->get_item = foundry_gir_node_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryGirNode, foundry_gir_node, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_CONTENT,
  PROP_NAME,
  PROP_NODE_TYPE,
  PROP_TAG_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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
foundry_gir_node_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  FoundryGirNode *self = FOUNDRY_GIR_NODE (object);

  switch (prop_id)
    {
    case PROP_CONTENT:
      g_value_set_string (value, foundry_gir_node_get_content (self));
      break;

    case PROP_NODE_TYPE:
      g_value_set_enum (value, foundry_gir_node_get_node_type (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_TAG_NAME:
      g_value_set_static_string (value, self->tag_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_gir_node_class_init (FoundryGirNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_gir_node_dispose;
  object_class->finalize = foundry_gir_node_finalize;
  object_class->get_property = foundry_gir_node_get_property;

  properties[PROP_CONTENT] =
    g_param_spec_string ("content", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NODE_TYPE] =
    g_param_spec_enum ("node-type", NULL, NULL,
                       FOUNDRY_TYPE_GIR_NODE_TYPE, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_TAG_NAME] =
    g_param_spec_string ("tag-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
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

FoundryGirNode *
_foundry_gir_node_traverse (FoundryGirNode     *node,
                            FoundryGirTraverse  traverse,
                            gpointer            user_data)
{
  FoundryGirTraverseResult res;

  res = traverse (node, user_data);

  if (res == FOUNDRY_GIR_TRAVERSE_STOP)
    return NULL;

  if (res == FOUNDRY_GIR_TRAVERSE_MATCH)
    return node;

  for (const GList *iter = node->children.head; iter; iter = iter->next)
    {
      FoundryGirNode *match = _foundry_gir_node_traverse (iter->data, traverse, user_data);

      if (match != NULL)
        return match;
    }

  return NULL;
}

/**
 * foundry_gir_node_find_ancestor:
 * @node: a [class@Foundry.GirNode]
 *
 * Returns: (transfer none) (nullable):
 */
FoundryGirNode *
foundry_gir_node_find_ancestor (FoundryGirNode     *node,
                                FoundryGirNodeType  type)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  node = foundry_gir_node_get_parent (node);
  while (node && node->type != type)
    node = node->parent;
  return node;
}

/**
 * foundry_gir_node_get_first_child:
 * @node: a [class@Foundry.GirNode]
 *
 * Returns: (transfer none) (nullable):
 */
FoundryGirNode *
foundry_gir_node_get_first_child (FoundryGirNode *node)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  return g_queue_peek_head (&node->children);
}

/**
 * foundry_gir_node_get_next_sibling:
 * @node: a [class@Foundry.GirNode]
 *
 * Returns: (transfer none) (nullable):
 */
FoundryGirNode *
foundry_gir_node_get_next_sibling (FoundryGirNode *node)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  return node->parent_link.next ? node->parent_link.next->data : NULL;
}

/**
 * foundry_gir_node_find_doc:
 * @node: a [class@Foundry.GirNode]
 *
 * Returns: (transfer none) (nullable):
 */
FoundryGirNode *
foundry_gir_node_find_doc (FoundryGirNode *node)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  for (const GList *iter = node->children.head; iter; iter = iter->next)
    {
      FoundryGirNode *child = iter->data;

      if (child->type == FOUNDRY_GIR_NODE_DOC)
        return child;
    }

  return NULL;
}

/**
 * foundry_gir_node_filter_typed:
 * @node: a [class@Foundry.GirNode]
 *
 * Returns: (transfer full):
 */
GListModel *
foundry_gir_node_filter_typed (FoundryGirNode     *node,
                               FoundryGirNodeType  type)
{
  GListStore *store = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIR_NODE (node), NULL);

  store = g_list_store_new (FOUNDRY_TYPE_GIR_NODE);

  for (const GList *iter = node->children.head; iter; iter = iter->next)
    {
      FoundryGirNode *child = iter->data;

      if (child->type == type)
        g_list_store_append (store, child);
    }

  return G_LIST_MODEL (store);
}

G_DEFINE_ENUM_TYPE (FoundryGirNodeType, foundry_gir_node_type,
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_UNKNOWN, "unknown"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_REPOSITORY, "repository"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_INCLUDE, "include"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_C_INCLUDE, "c-include"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_PACKAGE, "package"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_NAMESPACE, "namespace"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_ALIAS, "alias"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_ARRAY, "array"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_BITFIELD, "bitfield"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_CALLBACK, "callback"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_CLASS, "class"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_CLASS_METHOD, "class-method"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_CLASS_VIRTUAL_METHOD, "class-virtual-method"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_CLASS_PROPERTY, "class-property"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_CONSTRUCTOR, "constructor"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_CONSTANT, "constant"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_DOC, "doc"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_DOC_PARA, "doc-para"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_DOC_TEXT, "doc-text"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_ENUM, "enum"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_ENUM_MEMBER, "enum-member"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_FIELD, "field"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_FUNCTION, "function"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_FUNCTION_MACRO, "function-macro"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_GLIB_BOXED, "glib-boxed"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_GLIB_ERROR_DOMAIN, "glib-error-domain"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_GLIB_SIGNAL, "glib-signal"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_IMPLEMENTS, "implements"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_INSTANCE_PARAMETER, "instance-parameter"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_INTERFACE, "interface"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_METHOD, "method"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_NAMESPACE_FUNCTION, "namespace-function"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_PARAMETER, "parameter"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_PARAMETERS, "parameters"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_PREREQUISITE, "prerequisite"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_PROPERTY, "property"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_RECORD, "record"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_RETURN_VALUE, "return-value"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_SOURCE_POSITION, "source-position"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_TYPE, "type"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_UNION, "union"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_VARARGS, "varargs"),
                    G_DEFINE_ENUM_VALUE (FOUNDRY_GIR_NODE_VIRTUAL_METHOD, "virtual-method"))
