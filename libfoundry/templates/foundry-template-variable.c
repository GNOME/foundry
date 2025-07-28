/* foundry-template-variable.c
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

#include "foundry-template-variable.h"

G_DEFINE_ABSTRACT_TYPE (FoundryTemplateVariable, foundry_template_variable, G_TYPE_OBJECT)

static void
foundry_template_variable_class_init (FoundryTemplateVariableClass *klass)
{
}

static void
foundry_template_variable_init (FoundryTemplateVariable *self)
{
}

/**
 * foundry_template_variable_validate:
 * @self: a [class@Foundry.TemplateVariable]
 *
 * Checks if the variable contains valid input.
 *
 * Returns: a [class@Dex.Future] that resolves to %TRUE or rejects
 *   with error.
 */
DexFuture *
foundry_template_variable_validate (FoundryTemplateVariable *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_TEMPLATE_VARIABLE (self));

  if (FOUNDRY_TEMPLATE_VARIABLE_GET_CLASS (self)->validate)
    return FOUNDRY_TEMPLATE_VARIABLE_GET_CLASS (self)->validate (self);

  return dex_future_new_true ();
}
