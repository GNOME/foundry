/* foundry-test-suite.c
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

#include "foundry-test-suite-private.h"

/**
 * FoundryTestSuite:
 *
 * A grouping of [class@Foundry.Test]
 *
 * Since: 1.1
 */

struct _FoundryTestSuite
{
  GObject parent_instance;
  GListStore *tests;
  char *name;
};

static GType
foundry_test_suite_get_item_type (GListModel *model)
{
  FoundryTestSuite *self = FOUNDRY_TEST_SUITE (model);

  if (self->tests != NULL)
    return g_list_model_get_item_type (G_LIST_MODEL (self->tests));

  return G_TYPE_OBJECT;
}

static guint
foundry_test_suite_get_n_items (GListModel *model)
{
  FoundryTestSuite *self = FOUNDRY_TEST_SUITE (model);

  if (self->tests != NULL)
    return g_list_model_get_n_items (G_LIST_MODEL (self->tests));

  return 0;
}

static gpointer
foundry_test_suite_get_item (GListModel *model,
                             guint       position)
{
  FoundryTestSuite *self = FOUNDRY_TEST_SUITE (model);

  if (self->tests != NULL)
    return g_list_model_get_item (G_LIST_MODEL (self->tests), position);

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = foundry_test_suite_get_item_type;
  iface->get_n_items = foundry_test_suite_get_n_items;
  iface->get_item = foundry_test_suite_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundryTestSuite, foundry_test_suite, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_test_suite_finalize (GObject *object)
{
  FoundryTestSuite *self = (FoundryTestSuite *)object;

  g_clear_object (&self->tests);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (foundry_test_suite_parent_class)->finalize (object);
}

static void
foundry_test_suite_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryTestSuite *self = FOUNDRY_TEST_SUITE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_take_string (value, foundry_test_suite_dup_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_test_suite_class_init (FoundryTestSuiteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_test_suite_finalize;
  object_class->get_property = foundry_test_suite_get_property;

  /**
   * FoundryTestSuite:name:
   *
   * Since: 1.1
   */
  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_test_suite_init (FoundryTestSuite *self)
{
  self->tests = g_list_store_new (FOUNDRY_TYPE_TEST);

  g_signal_connect_object (self->tests,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

}

FoundryTestSuite *
_foundry_test_suite_new (const char *name)
{
  FoundryTestSuite *self;

  self = g_object_new (FOUNDRY_TYPE_TEST_SUITE, NULL);
  self->name = g_strdup (name);

  return self;
}

void
_foundry_test_suite_add (FoundryTestSuite *self,
                         FoundryTest      *test)
{
  g_return_if_fail (FOUNDRY_IS_TEST_SUITE (self));
  g_return_if_fail (FOUNDRY_IS_TEST (test));

  g_list_store_append (self->tests, test);
}

/**
 * foundry_test_suite_dup_name:
 * @self: a [class@Foundry.TestSuite]
 *
 * Gets the name of the suite if any.
 *
 * Returns: (transfer full) (nullable):
 *
 * Since: 1.1
 */
char *
foundry_test_suite_dup_name (FoundryTestSuite *self)
{
  g_return_val_if_fail (FOUNDRY_IS_TEST_SUITE (self), NULL);

  return g_strdup (self->name);
}
