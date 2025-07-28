/* foundry-template-manager.c
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

#include <libpeas.h>

#include "foundry-debug.h"
#include "foundry-model-manager.h"
#include "foundry-template-manager.h"
#include "foundry-template-provider.h"
#include "foundry-util-private.h"

struct _FoundryTemplateManager
{
  GObject           parent_instance;
  PeasExtensionSet *addins;
};

struct _FoundryTemplateManagerClass
{
  GObjectClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryTemplateManager, foundry_template_manager, G_TYPE_OBJECT)

static void
foundry_template_manager_constructed (GObject *object)
{
  FoundryTemplateManager *self = (FoundryTemplateManager *)object;

  G_OBJECT_CLASS (foundry_template_manager_parent_class)->constructed (object);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         FOUNDRY_TYPE_TEMPLATE_PROVIDER,
                                         NULL);
}

static void
foundry_template_manager_dispose (GObject *object)
{
  FoundryTemplateManager *self = (FoundryTemplateManager *)object;

  g_clear_object (&self->addins);

  G_OBJECT_CLASS (foundry_template_manager_parent_class)->dispose (object);
}

static void
foundry_template_manager_class_init (FoundryTemplateManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_template_manager_constructed;
  object_class->dispose = foundry_template_manager_dispose;
}

static void
foundry_template_manager_init (FoundryTemplateManager *self)
{
}

/**
 * foundry_template_manager_list_project_templates:
 * @self: a [class@Foundry.TemplateManager]
 *
 * Queries all [class@Foundry.TemplateProvider] for available
 * [class@Foundry.ProjectTemplate].
 *
 * The resulting module may not be fully populated by all providers
 * by time it resolves. You may await the completion of all providers
 * by awaiting [func@Foundry.list_model_await] for the completion
 * of all providers.
 *
 * This allows the consumer to get a dynamically populating list model
 * for user interfaces without delay.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.ProjectTemplate]
 */
DexFuture *
foundry_template_manager_list_project_templates (FoundryTemplateManager *self)
{
  g_autoptr(GPtrArray) futures = NULL;
  guint n_items;

  dex_return_error_if_fail (FOUNDRY_IS_TEMPLATE_MANAGER (self));

  if (self->addins == NULL)
    return foundry_future_new_not_supported ();

  futures = g_ptr_array_new_with_free_func (dex_unref);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->addins));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(FoundryTemplateProvider) provider = g_list_model_get_item (G_LIST_MODEL (self->addins), i);

      g_ptr_array_add (futures, foundry_template_provider_list_project_templates (provider));
    }

  return _foundry_flatten_list_model_new_from_futures (futures);
}

FoundryTemplateManager *
foundry_template_manager_new (void)
{
  return g_object_new (FOUNDRY_TYPE_TEMPLATE_MANAGER, NULL);
}
