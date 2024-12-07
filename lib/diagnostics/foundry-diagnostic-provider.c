/* foundry-diagnostic-provider.c
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

#include "foundry-diagnostic-provider-private.h"

G_DEFINE_ABSTRACT_TYPE (FoundryDiagnosticProvider, foundry_diagnostic_provider, FOUNDRY_TYPE_CONTEXTUAL)

static DexFuture *
foundry_diagnostic_provider_real_load (FoundryDiagnosticProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_diagnostic_provider_real_unload (FoundryDiagnosticProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_diagnostic_provider_real_diagnose (FoundryDiagnosticProvider *self,
                                           GFile                     *file)
{
  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "%s does not implement diagnose",
                                G_OBJECT_TYPE_NAME (self));
}

static void
foundry_diagnostic_provider_class_init (FoundryDiagnosticProviderClass *klass)
{
  klass->load = foundry_diagnostic_provider_real_load;
  klass->unload = foundry_diagnostic_provider_real_unload;
  klass->diagnose = foundry_diagnostic_provider_real_diagnose;
}

static void
foundry_diagnostic_provider_init (FoundryDiagnosticProvider *self)
{
}

DexFuture *
foundry_diagnostic_provider_load (FoundryDiagnosticProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DIAGNOSTIC_PROVIDER (self), NULL);

  return FOUNDRY_DIAGNOSTIC_PROVIDER_GET_CLASS (self)->load (self);
}

DexFuture *
foundry_diagnostic_provider_unload (FoundryDiagnosticProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DIAGNOSTIC_PROVIDER (self), NULL);

  return FOUNDRY_DIAGNOSTIC_PROVIDER_GET_CLASS (self)->unload (self);
}

/**
 * foundry_diagnostic_provider_dup_name:
 * @self: a #FoundryDiagnosticProvider
 *
 * Gets a name for the provider that is expected to be displayed to
 * users such as "Flatpak".
 *
 * Returns: (transfer full): the name of the provider
 */
char *
foundry_diagnostic_provider_dup_name (FoundryDiagnosticProvider *self)
{
  char *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_DIAGNOSTIC_PROVIDER (self), NULL);

  if (FOUNDRY_DIAGNOSTIC_PROVIDER_GET_CLASS (self)->dup_name)
    ret = FOUNDRY_DIAGNOSTIC_PROVIDER_GET_CLASS (self)->dup_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return g_steal_pointer (&ret);
}

/**
 * foundry_diagnostic_provider_diagnose:
 * @self: a #FoundryDiagnosticProvider
 *
 * Processes @file to extract diagnostics.
 *
 * If the buffer manager has modified contents for @file, they should be taken
 * into account when diagnosing.
 *
 * Returns: (transfer full): a #DexFuture that resolves to a #GListModel
 *   of #FoundryDiagnostic.
 */
DexFuture *
foundry_diagnostic_provider_diagnose (FoundryDiagnosticProvider *self,
                                      GFile                     *file)
{
  g_return_val_if_fail (FOUNDRY_IS_DIAGNOSTIC_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return FOUNDRY_DIAGNOSTIC_PROVIDER_GET_CLASS (self)->diagnose (self, file);
}
