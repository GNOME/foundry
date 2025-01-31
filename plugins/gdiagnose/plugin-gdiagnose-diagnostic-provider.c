/* plugin-gdiagnose-diagnostic-provider.c
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

#include "line-reader-private.h"

#include "plugin-gdiagnose-diagnostic-provider.h"

struct _PluginGdiagnoseDiagnosticProvider
{
  FoundryDiagnosticProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (PluginGdiagnoseDiagnosticProvider, plugin_gdiagnose_diagnostic_provider, FOUNDRY_TYPE_DIAGNOSTIC_PROVIDER)

static DexFuture *
plugin_gdiagnose_diagnostic_provider_diagnose_fiber (gpointer user_data)
{
  GBytes *contents = user_data;
  const char *data;
  const char *line;
  LineReader reader;
  gsize size;
  gsize len;

  g_assert (contents != NULL);

  data = g_bytes_get_data (contents, &size);


  return NULL;
}

static DexFuture *
plugin_gdiagnose_diagnostic_provider_diagnose (FoundryDiagnosticProvider *self,
                                               GFile                     *file,
                                               GBytes                    *contents,
                                               const char                *language)
{
  g_assert (PLUGIN_IS_GDIAGNOSE_DIAGNOSTIC_PROVIDER (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file || contents);

  if (contents == NULL || !g_strv_contains (FOUNDRY_STRV_INIT ("c"), language))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              plugin_gdiagnose_diagnostic_provider_diagnose_fiber,
                              g_bytes_ref (contents),
                              (GDestroyNotify) g_bytes_unref);
}

static void
plugin_gdiagnose_diagnostic_provider_class_init (PluginGdiagnoseDiagnosticProviderClass *klass)
{
  FoundryDiagnosticProviderClass *diagnostic_provider_class = FOUNDRY_DIAGNOSTIC_PROVIDER_CLASS (klass);

  diagnostic_provider_class->diagnose = plugin_gdiagnose_diagnostic_provider_diagnose;
}

static void
plugin_gdiagnose_diagnostic_provider_init (PluginGdiagnoseDiagnosticProvider *self)
{
}
