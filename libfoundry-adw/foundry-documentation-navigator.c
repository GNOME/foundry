/* foundry-documentation-navigator.c
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

#include "foundry-documentation-navigator.h"
#include "foundry-model-manager.h"
#include "foundry-util.h"

#include "foundry-documentation-intent.h"

struct _FoundryDocumentationNavigator
{
  FoundryPathNavigator  parent_instance;
  FoundryDocumentation *documentation;
};

enum {
  PROP_0,
  PROP_DOCUMENTATION,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryDocumentationNavigator, foundry_documentation_navigator, FOUNDRY_TYPE_PATH_NAVIGATOR)

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_documentation_navigator_find_parent_fiber (gpointer data)
{
  FoundryDocumentationNavigator *self = data;
  g_autoptr(FoundryDocumentation) parent = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_DOCUMENTATION_NAVIGATOR (self));

  if (!(context = foundry_path_navigator_dup_context (FOUNDRY_PATH_NAVIGATOR (self))))
    return foundry_future_new_disposed ();

  if (!(parent = dex_await_object (foundry_documentation_find_parent (self->documentation), &error)))
    {
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));
      else
        return dex_future_new_for_object (NULL);
    }

  return dex_future_new_for_object (foundry_documentation_navigator_new (context, parent));
}

static DexFuture *
foundry_documentation_navigator_find_parent (FoundryPathNavigator *navigator)
{
  FoundryDocumentationNavigator *self = FOUNDRY_DOCUMENTATION_NAVIGATOR (navigator);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_documentation_navigator_find_parent_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
foundry_documentation_navigator_list_children_fiber (gpointer data)
{
  FoundryDocumentationNavigator *self = data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GListModel) children = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GError) error = NULL;
  guint n_items;

  g_assert (FOUNDRY_IS_DOCUMENTATION_NAVIGATOR (self));

  if (!(context = foundry_path_navigator_dup_context (FOUNDRY_PATH_NAVIGATOR (self))))
    return foundry_future_new_disposed ();

  if (!(children = dex_await_object (foundry_documentation_find_children (self->documentation), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (foundry_list_model_await (children), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  store = g_list_store_new (FOUNDRY_TYPE_DOCUMENTATION_NAVIGATOR);
  n_items = g_list_model_get_n_items (children);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryDocumentation) child_documentation = NULL;
      FoundryDocumentationNavigator *child_navigator = NULL;

      child_documentation = g_list_model_get_item (children, i);
      child_navigator = foundry_documentation_navigator_new (context, child_documentation);

      g_list_store_append (store, child_navigator);
    }

  return dex_future_new_for_object (g_steal_pointer (&store));
}

static DexFuture *
foundry_documentation_navigator_list_children (FoundryPathNavigator *navigator)
{
  FoundryDocumentationNavigator *self = FOUNDRY_DOCUMENTATION_NAVIGATOR (navigator);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_documentation_navigator_list_children_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static DexFuture *
foundry_documentation_navigator_list_siblings_fiber (gpointer data)
{
  FoundryDocumentationNavigator *self = data;
  g_autoptr(FoundryPathNavigator) parent = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GListModel) children = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_DOCUMENTATION_NAVIGATOR (self));

  if (!(context = foundry_path_navigator_dup_context (FOUNDRY_PATH_NAVIGATOR (self))))
    return foundry_future_new_disposed ();

  if (!(parent = dex_await_object (foundry_path_navigator_find_parent (FOUNDRY_PATH_NAVIGATOR (self)), &error)))
    {
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));
      else
        return dex_future_new_for_object (g_list_store_new (FOUNDRY_TYPE_PATH_NAVIGATOR));
    }

  if (!(children = dex_await_object (foundry_path_navigator_list_children (parent), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  dex_await (foundry_list_model_await (children), NULL);

  return dex_future_new_for_object (g_steal_pointer (&children));
}

static DexFuture *
foundry_documentation_navigator_list_siblings (FoundryPathNavigator *navigator)
{
  FoundryDocumentationNavigator *self = FOUNDRY_DOCUMENTATION_NAVIGATOR (navigator);

  return dex_scheduler_spawn (NULL, 0,
                              foundry_documentation_navigator_list_siblings_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static char *
foundry_documentation_navigator_dup_title (FoundryPathNavigator *navigator)
{
  FoundryDocumentationNavigator *self = FOUNDRY_DOCUMENTATION_NAVIGATOR (navigator);

  if (self->documentation == NULL)
    return NULL;

  return foundry_documentation_dup_title (self->documentation);
}

static GIcon *
foundry_documentation_navigator_dup_icon (FoundryPathNavigator *navigator)
{
  FoundryDocumentationNavigator *self = FOUNDRY_DOCUMENTATION_NAVIGATOR (navigator);

  if (self->documentation == NULL)
    return NULL;

  return foundry_documentation_dup_icon (self->documentation);
}

static FoundryIntent *
foundry_documentation_navigator_dup_intent (FoundryPathNavigator *navigator)
{
  FoundryDocumentationNavigator *self = FOUNDRY_DOCUMENTATION_NAVIGATOR (navigator);
  g_autoptr(FoundryContext) context = NULL;

  if (self->documentation == NULL)
    return NULL;

  if (!(context = foundry_path_navigator_dup_context (FOUNDRY_PATH_NAVIGATOR (self))))
    return NULL;

  return foundry_documentation_intent_new (context, self->documentation);
}

static void
foundry_documentation_navigator_dispose (GObject *object)
{
  FoundryDocumentationNavigator *self = FOUNDRY_DOCUMENTATION_NAVIGATOR (object);

  g_clear_object (&self->documentation);

  G_OBJECT_CLASS (foundry_documentation_navigator_parent_class)->dispose (object);
}

static void
foundry_documentation_navigator_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  FoundryDocumentationNavigator *self = FOUNDRY_DOCUMENTATION_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_DOCUMENTATION:
      g_value_take_object (value, foundry_documentation_navigator_dup_documentation (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_documentation_navigator_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  FoundryDocumentationNavigator *self = FOUNDRY_DOCUMENTATION_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_DOCUMENTATION:
      self->documentation = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_documentation_navigator_class_init (FoundryDocumentationNavigatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryPathNavigatorClass *path_navigator_class = FOUNDRY_PATH_NAVIGATOR_CLASS (klass);

  object_class->dispose = foundry_documentation_navigator_dispose;
  object_class->get_property = foundry_documentation_navigator_get_property;
  object_class->set_property = foundry_documentation_navigator_set_property;

  path_navigator_class->list_siblings = foundry_documentation_navigator_list_siblings;
  path_navigator_class->find_parent = foundry_documentation_navigator_find_parent;
  path_navigator_class->list_children = foundry_documentation_navigator_list_children;
  path_navigator_class->dup_title = foundry_documentation_navigator_dup_title;
  path_navigator_class->dup_icon = foundry_documentation_navigator_dup_icon;
  path_navigator_class->dup_intent = foundry_documentation_navigator_dup_intent;

  properties[PROP_DOCUMENTATION] =
    g_param_spec_object ("documentation", NULL, NULL,
                         FOUNDRY_TYPE_DOCUMENTATION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_documentation_navigator_init (FoundryDocumentationNavigator *self)
{
}

/**
 * foundry_documentation_navigator_new:
 * @context: a [class@Foundry.Context]
 * @documentation: a [class@Foundry.Documentation]
 *
 * Creates a new documentation navigator for the given documentation.
 *
 * Returns: (transfer full): a new [class@Foundry.DocumentationNavigator]
 *
 * Since: 1.1
 */
FoundryDocumentationNavigator *
foundry_documentation_navigator_new (FoundryContext       *context,
                                     FoundryDocumentation *documentation)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (FOUNDRY_IS_DOCUMENTATION (documentation), NULL);

  return g_object_new (FOUNDRY_TYPE_DOCUMENTATION_NAVIGATOR,
                       "context", context,
                       "documentation", documentation,
                       NULL);
}

/**
 * foundry_documentation_navigator_dup_documentation:
 * @self: a [class@Foundry.DocumentationNavigator]
 *
 * Gets the documentation for this navigator.
 *
 * Returns: (transfer full): a [class@Foundry.Documentation]
 *
 * Since: 1.1
 */
FoundryDocumentation *
foundry_documentation_navigator_dup_documentation (FoundryDocumentationNavigator *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DOCUMENTATION_NAVIGATOR (self), NULL);

  return g_object_ref (self->documentation);
}
