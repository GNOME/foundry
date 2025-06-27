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
#include "foundry-diagnostic-private.h"
#include "foundry-diagnostic-range.h"
#include "foundry-markup.h"

struct _FoundryDiagnosticBuilder
{
  FoundryContext            *context;
  GFile                     *file;
  char                      *message;
  GListStore                *ranges;
  FoundryMarkup             *markup;
  guint                      line;
  guint                      line_offset;
  FoundryDiagnosticSeverity  severity;
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
  g_clear_object (&self->ranges);
  g_clear_pointer (&self->message, g_free);
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
  FoundryDiagnostic *result;

  g_return_val_if_fail (self != NULL, NULL);

  result = g_object_new (FOUNDRY_TYPE_DIAGNOSTIC, NULL);
  g_set_object (&result->file, self->file);
  g_set_str (&result->message, self->message);
  result->line = self->line;
  result->line_offset = self->line_offset;
  result->severity = self->severity;
  g_set_object (&result->ranges, G_LIST_MODEL (self->ranges));

  return result;
}

void
foundry_diagnostic_builder_set_file (FoundryDiagnosticBuilder *self,
                                     GFile                    *file)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (!file || G_IS_FILE (file));

  g_set_object (&self->file, file);
}

void
foundry_diagnostic_builder_set_path (FoundryDiagnosticBuilder *self,
                                     const char               *path)
{
  g_autoptr(GFile) file = NULL;

  g_return_if_fail (self != NULL);

  if (path != NULL)
    file = g_file_new_for_path (path);

  foundry_diagnostic_builder_set_file (self, file);
}

void
foundry_diagnostic_builder_set_markup (FoundryDiagnosticBuilder *self,
                                       FoundryMarkup            *markup)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (!markup || FOUNDRY_IS_MARKUP (markup));

  g_set_object (&self->markup, markup);
}

void
foundry_diagnostic_builder_take_markup (FoundryDiagnosticBuilder *self,
                                        FoundryMarkup            *markup)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (!markup || FOUNDRY_IS_MARKUP (markup));

  g_clear_object (&self->markup);
  self->markup = markup;
}

void
foundry_diagnostic_builder_set_message (FoundryDiagnosticBuilder *self,
                                        const char               *message)
{
  g_return_if_fail (self != NULL);

  g_set_str (&self->message, message);
}

void
foundry_diagnostic_builder_take_message (FoundryDiagnosticBuilder *self,
                                         char                     *message)
{
  g_return_if_fail (self != NULL);

  g_free (self->message);
  self->message = message;
}

void
foundry_diagnostic_builder_set_line (FoundryDiagnosticBuilder *self,
                                     guint                     line)
{
  g_return_if_fail (self != NULL);

  self->line = line;
}

void
foundry_diagnostic_builder_set_line_offset (FoundryDiagnosticBuilder *self,
                                            guint                     line_offset)
{
  g_return_if_fail (self != NULL);

  self->line_offset = line_offset;
}

void
foundry_diagnostic_builder_set_severity (FoundryDiagnosticBuilder  *self,
                                         FoundryDiagnosticSeverity  severity)
{
  g_return_if_fail (self != NULL);

  self->severity = severity;
}

void
foundry_diagnostic_builder_add_range (FoundryDiagnosticBuilder *self,
                                      guint                     start_line,
                                      guint                     start_col,
                                      guint                     end_line,
                                      guint                     end_col)
{
  g_autoptr(FoundryDiagnosticRange) range = NULL;

  g_return_if_fail (self != NULL);

  if (self->ranges == NULL)
    self->ranges = g_list_store_new (FOUNDRY_TYPE_DIAGNOSTIC_RANGE);

  range = g_object_new (FOUNDRY_TYPE_DIAGNOSTIC_RANGE,
                        "start-line", start_line,
                        "start-line-offset", start_col,
                        "end-line", end_line,
                        "end-line-offset", end_col,
                        NULL);

  g_list_store_append (self->ranges, range);
}
