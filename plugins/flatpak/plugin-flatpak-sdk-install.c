/* plugin-flatpak-sdk-install.c
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

#include <glib/gi18n-lib.h>

#include "plugin-flatpak.h"
#include "plugin-flatpak-sdk-private.h"

typedef struct _Install
{
  FoundryOperation    *operation;
  FlatpakInstallation *installation;
  FlatpakRef          *ref;
  FlatpakTransaction  *transaction;
  DexPromise          *promise;
} Install;

static void
install_finalize (gpointer data)
{
  Install *install = data;

  g_clear_object (&install->operation);
  g_clear_object (&install->installation);
  g_clear_object (&install->ref);
  g_clear_object (&install->transaction);
  dex_clear (&install->promise);
}

static void
install_unref (Install *install)
{
  return g_atomic_rc_box_release_full (install, install_finalize);
}

static Install *
install_ref (Install *install)
{
  return g_atomic_rc_box_acquire (install);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Install, install_unref)

static void
handle_notify_progress (FlatpakTransactionProgress *progress,
                        GParamSpec                 *pspec,
                        FoundryOperation           *operation)
{
  g_autofree char *status = NULL;
  double fraction;

  g_assert (FLATPAK_IS_TRANSACTION_PROGRESS (progress));
  g_assert (FOUNDRY_IS_OPERATION (operation));

  status = flatpak_transaction_progress_get_status (progress);
  fraction = flatpak_transaction_progress_get_progress (progress) / 100.;

  foundry_operation_set_subtitle (operation, status);
  foundry_operation_set_progress (operation, fraction);
}

static void
handle_new_operation (FlatpakTransaction          *object,
                      FlatpakTransactionOperation *operation,
                      FlatpakTransactionProgress  *progress,
                      FoundryOperation            *foundry_op)
{
  g_signal_connect_object (progress,
                           "notify",
                           G_CALLBACK (handle_notify_progress),
                           foundry_op,
                           0);

  handle_notify_progress (progress, NULL, foundry_op);
}

static gpointer
plugin_flatpak_sdk_install_thread (gpointer user_data)
{
  g_autoptr(Install) install = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (install != NULL);
  g_assert (FOUNDRY_IS_OPERATION (install->operation));
  g_assert (FLATPAK_IS_INSTALLATION (install->installation));
  g_assert (FLATPAK_IS_REF (install->ref));
  g_assert (FLATPAK_IS_TRANSACTION (install->transaction));
  g_assert (DEX_IS_PROMISE (install->promise));

  if (!flatpak_transaction_run (install->transaction,
                                dex_promise_get_cancellable (install->promise),
                                &error))
    dex_promise_reject (install->promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (install->promise, TRUE);

  return NULL;
}

static DexFuture *
plugin_flatpak_sdk_install_fiber (gpointer user_data)
{
  Install *install = user_data;
  g_autoptr(FlatpakTransaction) transaction = NULL;
  g_autoptr(FlatpakRemote) remote = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *ref_str = NULL;
  g_autofree char *title = NULL;

  g_assert (install != NULL);
  g_assert (FOUNDRY_IS_OPERATION (install->operation));
  g_assert (FLATPAK_IS_INSTALLATION (install->installation));
  g_assert (FLATPAK_IS_REF (install->ref));
  g_assert (DEX_IS_PROMISE (install->promise));

  ref_str = flatpak_ref_format_ref (install->ref);
  title = g_strdup_printf ("%s %s", _("Installing"), ref_str);

  foundry_operation_set_title (install->operation, title);

  if (!(transaction = flatpak_transaction_new_for_installation (install->installation, NULL, &error)) ||
      !(remote = plugin_flatpak_find_remote (install->installation, install->ref)) ||
      !(flatpak_transaction_add_install (transaction, flatpak_remote_get_name (remote), ref_str, NULL, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  flatpak_transaction_set_no_interaction (transaction, TRUE);

  if (flatpak_transaction_is_empty (transaction))
    {
      foundry_operation_complete (install->operation);
      return dex_future_new_true ();
    }

  g_signal_connect_object (transaction,
                           "new-operation",
                           G_CALLBACK (handle_new_operation),
                           install->operation,
                           0);

  /* Run transaction in a thread so we don't hold up the
   * thread pool with the long running action.
   */
  install->transaction = g_object_ref (transaction);
  g_thread_unref (g_thread_new ("[foundry-flatpak-install]",
                                plugin_flatpak_sdk_install_thread,
                                install_ref (install)));

  return DEX_FUTURE (dex_ref (install->promise));
}

DexFuture *
plugin_flatpak_sdk_install (FoundrySdk       *sdk,
                            FoundryOperation *operation)
{
  PluginFlatpakSdk *self = (PluginFlatpakSdk *)sdk;
  Install *install;

  g_assert (PLUGIN_IS_FLATPAK_SDK (self));
  g_assert (FOUNDRY_IS_OPERATION (operation));

  install = g_atomic_rc_box_new0 (Install);
  install->operation = g_object_ref (operation);
  install->installation = g_object_ref (self->installation);
  install->ref = g_object_ref (self->ref);
  install->promise = dex_promise_new_cancellable ();

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (), 0,
                              plugin_flatpak_sdk_install_fiber,
                              install,
                              (GDestroyNotify) install_unref);
}
