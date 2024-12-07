/* foundry-service.c
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

#include "foundry-service-private.h"

G_DEFINE_ABSTRACT_TYPE (FoundryService, foundry_service, FOUNDRY_TYPE_CONTEXTUAL)
G_DEFINE_QUARK (foundry_service_error, foundry_service_error)

static DexFuture *
foundry_service_real_start (FoundryService *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_service_real_stop (FoundryService *self)
{
  return dex_future_new_true ();
}

static void
foundry_service_finalize (GObject *object)
{
  FoundryService *self = (FoundryService *)object;

  dex_clear (&self->started);
  dex_clear (&self->stopped);

  G_OBJECT_CLASS (foundry_service_parent_class)->finalize (object);
}

static void
foundry_service_class_init (FoundryServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_service_finalize;

  klass->start = foundry_service_real_start;
  klass->stop = foundry_service_real_stop;
}

static void
foundry_service_init (FoundryService *self)
{
  self->started = dex_promise_new ();
  self->stopped = dex_promise_new ();
}

/**
 * foundry_service_when_ready:
 * @self: a #FoundryService
 *
 * Gets a future that resolves when the service has started.
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_service_when_ready (FoundryService *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SERVICE (self), NULL);

  if (self->has_stopped)
    return dex_future_new_reject (FOUNDRY_SERVICE_ERROR,
                                  FOUNDRY_SERVICE_ERROR_ALREADY_STOPPED,
                                  "Service has already been shutdown");

  return dex_ref (self->started);
}

/**
 * foundry_service_when_shutdown:
 * @self: a #FoundryService
 *
 * Gets a future that resolves when the service has shutdown.
 *
 * Returns: (transfer full): A #DexFuture
 */
DexFuture *
foundry_service_when_shutdown (FoundryService *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SERVICE (self), NULL);

  return dex_ref (self->stopped);
}

static DexFuture *
foundry_service_propagate (DexFuture *from,
                           gpointer   user_data)
{
  DexPromise *to = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (from));
  g_assert (DEX_IS_PROMISE (to));

  if ((value = dex_future_get_value (from, &error)))
    dex_promise_resolve (to, value);
  else
    dex_promise_reject (to, g_steal_pointer (&error));

  return NULL;
}

DexFuture *
foundry_service_start (FoundryService *self)
{
  DexFuture *future;

  g_return_val_if_fail (FOUNDRY_IS_SERVICE (self), NULL);

  if (self->has_started)
    return dex_future_new_reject (FOUNDRY_SERVICE_ERROR,
                                  FOUNDRY_SERVICE_ERROR_ALREADY_STARTED,
                                  "Service already started");

  self->has_started = TRUE;

  g_debug ("Starting service %s", G_OBJECT_TYPE_NAME (self));

  future = FOUNDRY_SERVICE_GET_CLASS (self)->start (self);
  future = dex_future_finally (future,
                               foundry_service_propagate,
                               dex_ref (self->started),
                               dex_unref);

  return future;
}

DexFuture *
foundry_service_stop (FoundryService *self)
{
  DexFuture *future;

  g_return_val_if_fail (FOUNDRY_IS_SERVICE (self), NULL);

  if (self->has_stopped)
    return dex_future_new_reject (FOUNDRY_SERVICE_ERROR,
                                  FOUNDRY_SERVICE_ERROR_ALREADY_STOPPED,
                                  "Service already stopped");

  self->has_stopped = TRUE;

  g_debug ("Stopping service %s", G_OBJECT_TYPE_NAME (self));

  future = FOUNDRY_SERVICE_GET_CLASS (self)->stop (self);
  future = dex_future_finally (future,
                               foundry_service_propagate,
                               dex_ref (self->stopped),
                               dex_unref);

  return future;
}
