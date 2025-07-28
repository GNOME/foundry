/* foundry-template-provider.c
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

#include "foundry-template-provider.h"
#include "foundry-util.h"

G_DEFINE_ABSTRACT_TYPE (FoundryTemplateProvider, foundry_template_provider, G_TYPE_OBJECT)

static void
foundry_template_provider_class_init (FoundryTemplateProviderClass *klass)
{
}

static void
foundry_template_provider_init (FoundryTemplateProvider *self)
{
}

/**
 * foundry_template_provider_list_project_templates:
 * @self: a [class@Foundry.TemplateProvider]
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of [class@Foundry.ProjectTemplate] or rejects
 *   with error.
 */
DexFuture *
foundry_template_provider_list_project_templates (FoundryTemplateProvider *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEMPLATE_PROVIDER (self));

  if (FOUNDRY_TEMPLATE_PROVIDER_GET_CLASS (self)->list_project_templates)
    return FOUNDRY_TEMPLATE_PROVIDER_GET_CLASS (self)->list_project_templates (self);

  return foundry_future_new_not_supported ();
}
