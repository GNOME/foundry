/* foundry-gir.c
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

#include "foundry-gir.h"

#include "foundry-gir-node-private.h"

struct _FoundryGir
{
  GObject         parent_instance;
  GFile          *file;
  FoundryGirNode *repository;
};

typedef struct
{
  GPtrArray      *stack;
  FoundryGirNode *root;
} FoundryGirParserState;

G_DEFINE_FINAL_TYPE (FoundryGir, foundry_gir, G_TYPE_OBJECT)

static void
foundry_gir_dispose (GObject *object)
{
  FoundryGir *self = FOUNDRY_GIR (object);

  g_clear_object (&self->repository);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (foundry_gir_parent_class)->dispose (object);
}

static void
foundry_gir_class_init (FoundryGirClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_gir_dispose;
}

static void
foundry_gir_init (FoundryGir *self)
{
}

GQuark
foundry_gir_error_quark (void)
{
  return g_quark_from_static_string ("foundry-gir-error");
}

static FoundryGirNodeType
foundry_gir_node_type_from_element (const char *element_name)
{
  static const struct
  {
    const char *element;
    FoundryGirNodeType type;
  } map[] = {
    { "repository",         FOUNDRY_GIR_NODE_REPOSITORY },
    { "include",            FOUNDRY_GIR_NODE_INCLUDE },
    { "c:include",          FOUNDRY_GIR_NODE_C_INCLUDE },
    { "package",            FOUNDRY_GIR_NODE_PACKAGE },
    { "namespace",          FOUNDRY_GIR_NODE_NAMESPACE },
    { "alias",              FOUNDRY_GIR_NODE_ALIAS },
    { "array",              FOUNDRY_GIR_NODE_ARRAY },
    { "bitfield",           FOUNDRY_GIR_NODE_BITFIELD },
    { "callback",           FOUNDRY_GIR_NODE_CALLBACK },
    { "class",              FOUNDRY_GIR_NODE_CLASS },
    { "method",             FOUNDRY_GIR_NODE_METHOD },
    { "constructor",        FOUNDRY_GIR_NODE_CONSTRUCTOR },
    { "virtual-method",     FOUNDRY_GIR_NODE_VIRTUAL_METHOD },
    { "function",           FOUNDRY_GIR_NODE_FUNCTION },
    { "function-macro",     FOUNDRY_GIR_NODE_FUNCTION_MACRO },
    { "constant",           FOUNDRY_GIR_NODE_CONSTANT },
    { "doc:doc",            FOUNDRY_GIR_NODE_DOC },
    { "doc:para",           FOUNDRY_GIR_NODE_DOC_PARA },
    { "doc:text",           FOUNDRY_GIR_NODE_DOC_TEXT },
    { "enumeration",        FOUNDRY_GIR_NODE_ENUM },
    { "member",             FOUNDRY_GIR_NODE_ENUM_MEMBER },
    { "field",              FOUNDRY_GIR_NODE_FIELD },
    { "property",           FOUNDRY_GIR_NODE_PROPERTY },
    { "glib:property",      FOUNDRY_GIR_NODE_PROPERTY },
    { "glib:signal",        FOUNDRY_GIR_NODE_GLIB_SIGNAL },
    { "glib:error-domain",  FOUNDRY_GIR_NODE_GLIB_ERROR_DOMAIN },
    { "glib:boxed",         FOUNDRY_GIR_NODE_GLIB_BOXED },
    { "implements",         FOUNDRY_GIR_NODE_IMPLEMENTS },
    { "prerequisite",       FOUNDRY_GIR_NODE_PREREQUISITE },
    { "parameters",         FOUNDRY_GIR_NODE_PARAMETERS },
    { "parameter",          FOUNDRY_GIR_NODE_PARAMETER },
    { "instance-parameter", FOUNDRY_GIR_NODE_INSTANCE_PARAMETER },
    { "return-value",       FOUNDRY_GIR_NODE_RETURN_VALUE },
    { "type",               FOUNDRY_GIR_NODE_TYPE },
    { "union",              FOUNDRY_GIR_NODE_UNION },
    { "record",             FOUNDRY_GIR_NODE_RECORD },
    { "interface",          FOUNDRY_GIR_NODE_INTERFACE },
    { "source-position",    FOUNDRY_GIR_NODE_SOURCE_POSITION },
    { "varargs",            FOUNDRY_GIR_NODE_VARARGS },
  };

  for (guint i = 0; i < G_N_ELEMENTS (map); i++)
    {
      if (g_strcmp0 (element_name, map[i].element) == 0)
        return map[i].type;
    }

  if (g_str_has_prefix (element_name, "doc:") ||
      g_str_equal (element_name, "doc"))
    return FOUNDRY_GIR_NODE_DOC;

  return FOUNDRY_GIR_NODE_UNKNOWN;
}

static inline FoundryGirNode *
foundry_gir_parser_state_peek (FoundryGirParserState *state)
{
  if (state->stack->len == 0)
    return NULL;

  return g_ptr_array_index (state->stack, state->stack->len - 1);
}

static void
foundry_gir_parser_start_element (GMarkupParseContext  *context,
                                  const char           *element_name,
                                  const char          **attribute_names,
                                  const char          **attribute_values,
                                  gpointer              user_data,
                                  GError              **error)
{
  FoundryGirParserState *state = user_data;
  FoundryGirNode *parent = foundry_gir_parser_state_peek (state);
  FoundryGirNodeType node_type = foundry_gir_node_type_from_element (element_name);
  g_autoptr(FoundryGirNode) node = NULL;

  node = foundry_gir_node_new (node_type, element_name);

  for (guint i = 0; attribute_names[i] != NULL; i++)
    _foundry_gir_node_add_attribute (node, attribute_names[i], attribute_values[i]);

  if (parent == NULL)
    {
      if (state->root != NULL)
        {
          g_set_error (error,
                       FOUNDRY_GIR_ERROR,
                       FOUNDRY_GIR_ERROR_PARSE,
                       "Multiple root elements encountered, expected a single <repository>");
          return;
        }

      state->root = g_object_ref (node);
    }
  else
    {
      _foundry_gir_node_add_child (parent, node);
    }

  g_ptr_array_add (state->stack, g_steal_pointer (&node));
}

static void
foundry_gir_parser_end_element (GMarkupParseContext  *context,
                                const char           *element_name,
                                gpointer              user_data,
                                GError              **error)
{
  FoundryGirParserState *state = user_data;
  FoundryGirNode *node = NULL;

  if (state->stack->len == 0)
    {
      g_set_error (error,
                   FOUNDRY_GIR_ERROR,
                   FOUNDRY_GIR_ERROR_PARSE,
                   "Unexpected closing element </%s>", element_name);
      return;
    }

  node = g_ptr_array_index (state->stack, state->stack->len - 1);

  if (g_strcmp0 (foundry_gir_node_get_tag_name (node), element_name) != 0)
    {
      g_set_error (error,
                   FOUNDRY_GIR_ERROR,
                   FOUNDRY_GIR_ERROR_PARSE,
                   "Mismatched closing element </%s>, expected </%s>",
                   element_name,
                   foundry_gir_node_get_tag_name (node));
      return;
    }

  g_ptr_array_remove_index (state->stack, state->stack->len - 1);
  g_object_unref (node);
}

static void
foundry_gir_parser_text (GMarkupParseContext  *context,
                         const char           *text,
                         gsize                 text_len,
                         gpointer              user_data,
                         GError              **error)
{
  FoundryGirParserState *state = user_data;
  FoundryGirNode *node = foundry_gir_parser_state_peek (state);

  if (node == NULL)
    return;

  _foundry_gir_node_append_text (node, text, text_len);
}

static const GMarkupParser gir_markup_parser = {
  .start_element = foundry_gir_parser_start_element,
  .end_element = foundry_gir_parser_end_element,
  .text = foundry_gir_parser_text,
};

static gboolean
foundry_gir_parse (FoundryGirParserState  *state,
                   GBytes                 *bytes,
                   GError                **error)
{
  g_autoptr(GMarkupParseContext) context = NULL;
  const char *data;
  gboolean success = FALSE;
  gsize length;

  g_assert (state != NULL);
  g_assert (bytes != NULL);

  data = (const char *)g_bytes_get_data (bytes, &length);
  context = g_markup_parse_context_new (&gir_markup_parser,
                                        G_MARKUP_TREAT_CDATA_AS_TEXT,
                                        state,
                                        NULL);

  if (length > (gsize)SSIZE_MAX)
    {
      g_set_error (error,
                   FOUNDRY_GIR_ERROR,
                   FOUNDRY_GIR_ERROR_PARSE,
                   "Input length exceeds supported range");
      return FALSE;
    }

  success = g_markup_parse_context_parse (context, data, length, error);

  if (success)
    success = g_markup_parse_context_end_parse (context, error);

  if (!success)
    return FALSE;

  if (state->root == NULL)
    {
      g_set_error (error,
                   FOUNDRY_GIR_ERROR,
                   FOUNDRY_GIR_ERROR_PARSE,
                   "Missing <repository> root element");
      return FALSE;
    }

  if (foundry_gir_node_get_node_type (state->root) != FOUNDRY_GIR_NODE_REPOSITORY)
    {
      g_set_error (error,
                   FOUNDRY_GIR_ERROR,
                   FOUNDRY_GIR_ERROR_PARSE,
                   "Unexpected root element <%s>, expected <repository>",
                   foundry_gir_node_get_tag_name (state->root));
      return FALSE;
    }

  if (state->stack->len != 0)
    {
      g_set_error (error,
                   FOUNDRY_GIR_ERROR,
                   FOUNDRY_GIR_ERROR_PARSE,
                   "Unbalanced XML elements while parsing GIR");
      return FALSE;
    }

  return TRUE;
}

static DexFuture *
foundry_gir_new_fiber (gpointer data)
{
  GFile *file = data;
  g_autoptr(FoundryGir) gir = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;
  FoundryGirParserState state = {0};
  gboolean success = FALSE;

  g_assert (G_IS_FILE (file));

  if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  state.stack = g_ptr_array_new ();
  success = foundry_gir_parse (&state, bytes, &error);

  if (success)
    {
      gir = g_object_new (FOUNDRY_TYPE_GIR, NULL);
      gir->file = g_object_ref (file);
      gir->repository = g_object_ref (state.root);
    }

  g_ptr_array_set_free_func (state.stack, g_object_unref);
  g_clear_pointer (&state.stack, g_ptr_array_unref);
  g_clear_object (&state.root);

  if (success)
    return dex_future_new_take_object (g_steal_pointer (&gir));

  return dex_future_new_for_error (g_steal_pointer (&error));
}

/**
 * foundry_gir_new:
 * @file: a [iface@Gio.File]
 *
 * Parses @file to create a new in-memory representation as a
 * [class@Foundry.Gir].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.Gir] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_gir_new (GFile *file)
{
  dex_return_error_if_fail (G_IS_FILE (file));

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              foundry_gir_new_fiber,
                              g_object_ref (file),
                              g_object_unref);
}

/**
 * foundry_gir_new_for_path:
 * @path: the path to the file
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Foundry.Gir] or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_gir_new_for_path (const char *path)
{
  g_autoptr(GFile) file = NULL;

  dex_return_error_if_fail (path != NULL);

  file = g_file_new_for_path (path);

  return foundry_gir_new (file);
}

/**
 * foundry_gir_get_file:
 * @gir: a [class@Foundry.Gir]
 *
 * Returns: (transfer none) (not nullable):
 *
 * Since: 1.1
 */
GFile *
foundry_gir_get_file (FoundryGir *gir)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR (gir), NULL);

  return gir->file;
}

/**
 * foundry_gir_get_repository:
 * @gir: a [class@Foundry.Gir]
 *
 * Returns: (transfer none) (not nullable):
 *
 * Since: 1.1
 */
FoundryGirNode *
foundry_gir_get_repository (FoundryGir *gir)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR (gir), NULL);

  return gir->repository;
}

/**
 * foundry_gir_get_namespace:
 * @gir: a [class@Foundry.Gir]
 *
 * Returns: (transfer none) (nullable):
 *
 * Since: 1.1
 */
FoundryGirNode *
foundry_gir_get_namespace (FoundryGir *gir,
                           const char *namespace_name)
{
  FoundryGirNode *repository = NULL;

  g_return_val_if_fail (FOUNDRY_IS_GIR (gir), NULL);
  g_return_val_if_fail (namespace_name != NULL, NULL);

  repository = foundry_gir_get_repository (gir);

  if (repository == NULL)
    return NULL;

  return foundry_gir_node_find_child (repository,
                                      FOUNDRY_GIR_NODE_NAMESPACE,
                                      namespace_name);
}

/**
 * foundry_gir_list_namespaces:
 * @gir: a [class@Foundry.Gir]
 * @n_namespaces: (out): location for number of namespaces
 *
 * Returns: (transfer container) (array length=n_namespaces):
 *
 * Since: 1.1
 */
FoundryGirNode **
foundry_gir_list_namespaces (FoundryGir *gir,
                             guint      *n_namespaces)
{
  FoundryGirNode *repository;

  g_return_val_if_fail (FOUNDRY_IS_GIR (gir), NULL);
  g_return_val_if_fail (n_namespaces != NULL, NULL);

  *n_namespaces = 0;

  if (!(repository = foundry_gir_get_repository (gir)))
    return NULL;

  return (FoundryGirNode **)foundry_gir_node_list_children_typed (repository, FOUNDRY_GIR_NODE_NAMESPACE, n_namespaces);
}
