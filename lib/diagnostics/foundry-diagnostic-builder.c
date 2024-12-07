/* foundry-diagnostic-builder.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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
#include "foundry-diagnostic-builder.h"

struct _FoundryDiagnosticBuilder
{
  FoundryContext *context;
  GFile          *file;
  char           *path;
};

G_DEFINE_BOXED_TYPE (FoundryDiagnosticBuilder,
                     foundry_diagnostic_builder,
                     foundry_diagnostic_builder_ref,
                     foundry_diagnostic_builder_unref)

FoundryDiagnosticBuilder *
foundry_diagnostic_builder_new (FoundryContext *context)
{
  FoundryDiagnosticBuilder *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  self = g_atomic_rc_box_new0 (FoundryDiagnosticBuilder);
  self->context = g_object_ref (context);

  return self;
}

FoundryDiagnosticBuilder *
foundry_diagnostic_builder_ref (FoundryDiagnosticBuilder *self)
{
  return g_atomic_rc_box_acquire (self);
}

static void
foundry_diagnostic_builder_finalize (gpointer data)
{
  FoundryDiagnosticBuilder *self = data;

  g_clear_object (&self->context);
  g_clear_object (&self->file);
  g_clear_pointer (&self->path, g_free);
}

void
foundry_diagnostic_builder_unref (FoundryDiagnosticBuilder *self)
{
  g_atomic_rc_box_release_full (self, foundry_diagnostic_builder_finalize);
}

/**
 * foundry_diagnostic_builder_end:
 * @self: a #FoundryDiagnosticBuilder
 *
 * Returns: (transfer full) (nullable): a #FoundryDiagnostic or %NULL
 */
FoundryDiagnostic *
foundry_diagnostic_builder_end (FoundryDiagnosticBuilder *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return NULL;
}
