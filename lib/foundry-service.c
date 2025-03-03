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

typedef struct
{
  DexPromise *started;
  DexPromise *stopped;
  guint       has_started : 1;
  guint       has_stopped : 1;
} FoundryServicePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryService, foundry_service, FOUNDRY_TYPE_CONTEXTUAL)
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
  FoundryServicePrivate *priv = foundry_service_get_instance_private (self);

  dex_clear (&priv->started);
  dex_clear (&priv->stopped);

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
  FoundryServicePrivate *priv = foundry_service_get_instance_private (self);

  priv->started = dex_promise_new ();
  priv->stopped = dex_promise_new ();
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
  FoundryServicePrivate *priv = foundry_service_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SERVICE (self), NULL);

  if (priv->has_stopped)
    return dex_future_new_reject (FOUNDRY_SERVICE_ERROR,
                                  FOUNDRY_SERVICE_ERROR_ALREADY_STOPPED,
                                  "Service has already been shutdown");

  return dex_ref (priv->started);
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
  FoundryServicePrivate *priv = foundry_service_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SERVICE (self), NULL);

  return dex_ref (priv->stopped);
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
  FoundryServicePrivate *priv = foundry_service_get_instance_private (self);
  DexFuture *future;

  g_return_val_if_fail (FOUNDRY_IS_SERVICE (self), NULL);

  if (priv->has_started)
    return dex_future_new_reject (FOUNDRY_SERVICE_ERROR,
                                  FOUNDRY_SERVICE_ERROR_ALREADY_STARTED,
                                  "Service already started");

  priv->has_started = TRUE;

  g_debug ("Starting service %s", G_OBJECT_TYPE_NAME (self));

  future = FOUNDRY_SERVICE_GET_CLASS (self)->start (self);
  future = dex_future_finally (future,
                               foundry_service_propagate,
                               dex_ref (priv->started),
                               dex_unref);

  return future;
}

DexFuture *
foundry_service_stop (FoundryService *self)
{
  FoundryServicePrivate *priv = foundry_service_get_instance_private (self);
  DexFuture *future;

  g_return_val_if_fail (FOUNDRY_IS_SERVICE (self), NULL);

  if (priv->has_stopped)
    return dex_future_new_reject (FOUNDRY_SERVICE_ERROR,
                                  FOUNDRY_SERVICE_ERROR_ALREADY_STOPPED,
                                  "Service already stopped");

  priv->has_stopped = TRUE;

  g_debug ("Stopping service %s", G_OBJECT_TYPE_NAME (self));

  future = FOUNDRY_SERVICE_GET_CLASS (self)->stop (self);
  future = dex_future_finally (future,
                               foundry_service_propagate,
                               dex_ref (priv->stopped),
                               dex_unref);

  return future;
}

void
foundry_service_class_set_action_prefix (FoundryServiceClass *service_class,
                                         const char          *action_prefix)
{
  g_return_if_fail (FOUNDRY_IS_SERVICE_CLASS (service_class));

  service_class->action_prefix = g_intern_string (action_prefix);
}
