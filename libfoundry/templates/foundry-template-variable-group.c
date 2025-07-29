/* foundry-template-variable-group.c
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

#include "foundry-template-variable-group.h"
#include "foundry-util.h"

struct _FoundryTemplateVariableGroup
{
  FoundryTemplateVariable parent_instance;
  GPtrArray *variables;
  char *title;
};

G_DEFINE_FINAL_TYPE (FoundryTemplateVariableGroup, foundry_template_variable_group, FOUNDRY_TYPE_TEMPLATE_VARIABLE)

static DexFuture *
foundry_template_variable_group_validate (FoundryTemplateVariable *variable)
{
  FoundryTemplateVariableGroup *self = FOUNDRY_TEMPLATE_VARIABLE_GROUP (variable);
  g_autoptr(GPtrArray) futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < self->variables->len; i++)
    g_ptr_array_add (futures, foundry_template_variable_validate (self->variables->pdata[i]));

  if (futures->len)
    return foundry_future_all (futures);

  return dex_future_new_true ();
}

static char *
foundry_template_variable_group_dup_title (FoundryTemplateVariable *variable)
{
  FoundryTemplateVariableGroup *self = FOUNDRY_TEMPLATE_VARIABLE_GROUP (variable);

  return g_strdup (self->title);
}

static void
foundry_template_variable_group_finalize (GObject *object)
{
  FoundryTemplateVariableGroup *self = (FoundryTemplateVariableGroup *)object;

  g_clear_pointer (&self->variables, g_ptr_array_unref);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (foundry_template_variable_group_parent_class)->finalize (object);
}

static void
foundry_template_variable_group_class_init (FoundryTemplateVariableGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryTemplateVariableClass *variable_class = FOUNDRY_TEMPLATE_VARIABLE_CLASS (klass);

  object_class->finalize = foundry_template_variable_group_finalize;

  variable_class->dup_title = foundry_template_variable_group_dup_title;
  variable_class->validate = foundry_template_variable_group_validate;
}

static void
foundry_template_variable_group_init (FoundryTemplateVariableGroup *self)
{
  self->variables = g_ptr_array_new_with_free_func (g_object_unref);
}

FoundryTemplateVariable *
foundry_template_variable_group_new (FoundryTemplateVariable **variables,
                                     guint                     n_variables,
                                     const char               *title)
{
  FoundryTemplateVariableGroup *self;

  g_return_val_if_fail (variables != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_TEMPLATE_VARIABLE_GROUP, NULL);
  self->title = g_strdup (title);

  for (guint i = 0; i < n_variables; i++)
    g_ptr_array_add (self->variables, g_object_ref (variables[i]));

  return FOUNDRY_TEMPLATE_VARIABLE (self);
}
