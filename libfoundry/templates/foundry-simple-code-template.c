/* foundry-simple-code-template.c
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

#include "foundry-context.h"
#include "foundry-internal-template-private.h"
#include "foundry-simple-code-template.h"

struct _FoundrySimpleCodeTemplate
{
  FoundryCodeTemplate  parent_instance;
  FoundryTemplate     *template;
};

G_DEFINE_FINAL_TYPE (FoundrySimpleCodeTemplate, foundry_simple_code_template, FOUNDRY_TYPE_CODE_TEMPLATE)

static char *
foundry_simple_code_template_dup_id (FoundryTemplate *template)
{
  return foundry_template_dup_id (FOUNDRY_SIMPLE_CODE_TEMPLATE (template)->template);
}

static char *
foundry_simple_code_template_dup_description (FoundryTemplate *template)
{
  return foundry_template_dup_description (FOUNDRY_SIMPLE_CODE_TEMPLATE (template)->template);
}

static FoundryInput *
foundry_simple_code_template_dup_input (FoundryTemplate *template)
{
  return foundry_template_dup_input (FOUNDRY_SIMPLE_CODE_TEMPLATE (template)->template);
}

static DexFuture *
foundry_simple_code_template_expand (FoundryTemplate *template)
{
  return foundry_template_expand (FOUNDRY_SIMPLE_CODE_TEMPLATE (template)->template);
}

static void
foundry_simple_code_template_finalize (GObject *object)
{
  FoundrySimpleCodeTemplate *self = (FoundrySimpleCodeTemplate *)object;

  g_clear_object (&self->template);

  G_OBJECT_CLASS (foundry_simple_code_template_parent_class)->finalize (object);
}

static void
foundry_simple_code_template_class_init (FoundrySimpleCodeTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryTemplateClass *template_class = FOUNDRY_TEMPLATE_CLASS (klass);

  object_class->finalize = foundry_simple_code_template_finalize;

  template_class->dup_id = foundry_simple_code_template_dup_id;
  template_class->dup_description = foundry_simple_code_template_dup_description;
  template_class->dup_input = foundry_simple_code_template_dup_input;
  template_class->expand = foundry_simple_code_template_expand;
}

static void
foundry_simple_code_template_init (FoundrySimpleCodeTemplate *self)
{
}

static DexFuture *
foundry_simple_code_template_wrap (DexFuture *completed,
                                   gpointer   data)
{
  FoundrySimpleCodeTemplate *self;

  self = g_object_new (FOUNDRY_TYPE_SIMPLE_CODE_TEMPLATE, NULL);
  self->template = dex_await_object (dex_ref (completed), NULL);

  return dex_future_new_take_object (self);
}

DexFuture *
foundry_simple_code_template_new (FoundryContext *context,
                                  GFile          *file)
{
  dex_return_error_if_fail (!context || FOUNDRY_IS_CONTEXT (context));
  dex_return_error_if_fail (G_IS_FILE (file));

  return dex_future_then (foundry_internal_template_new (context, file),
                          foundry_simple_code_template_wrap,
                          NULL, NULL);
}
