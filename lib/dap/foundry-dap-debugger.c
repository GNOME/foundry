/* foundry-dap-debugger.c
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

#include "foundry-command.h"
#include "foundry-dap-client-private.h"
#include "foundry-dap-debugger.h"
#include "foundry-dap-event.h"
#include "foundry-dap-output-event.h"
#include "foundry-debugger-target.h"
#include "foundry-debugger-target-command.h"
#include "foundry-debugger-target-process.h"
#include "foundry-util-private.h"

typedef struct
{
  GIOStream        *stream;
  GSubprocess      *subprocess;
  FoundryDapClient *client;
} FoundryDapDebuggerPrivate;

enum {
  PROP_0,
  PROP_STREAM,
  PROP_SUBPROCESS,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryDapDebugger, foundry_dap_debugger, FOUNDRY_TYPE_DEBUGGER)

static GParamSpec *properties[N_PROPS];

static void
foundry_dap_debugger_client_event_cb (FoundryDapDebugger *self,
                                      FoundryDapEvent    *event,
                                      FoundryDapClient   *client)
{
  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));
  g_assert (FOUNDRY_IS_DAP_EVENT (event));
  g_assert (FOUNDRY_IS_DAP_CLIENT (client));

  if (FOUNDRY_IS_DAP_OUTPUT_EVENT (event))
    {
    }
}

static DexFuture *
foundry_dap_debugger_exited (DexFuture *future,
                             gpointer   user_data)
{
  GWeakRef *wr = user_data;
  FoundryDapDebuggerPrivate *priv;
  g_autoptr(FoundryDapDebugger) self = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));

  if (!(self = g_weak_ref_get (wr)))
    return dex_future_new_true ();

  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));

  priv = foundry_dap_debugger_get_instance_private (self);

  if (!dex_await (dex_ref (future), &error))
    {
      if (priv->stream != NULL)
        g_io_stream_close (priv->stream, NULL, NULL);
    }

  return dex_ref (future);
}

static void
foundry_dap_debugger_constructed (GObject *object)
{
  FoundryDapDebugger *self = (FoundryDapDebugger *)object;
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  G_OBJECT_CLASS (foundry_dap_debugger_parent_class)->constructed (object);

  if (priv->subprocess != NULL)
    dex_future_disown (dex_future_finally (dex_subprocess_wait_check (priv->subprocess),
                                           foundry_dap_debugger_exited,
                                           foundry_weak_ref_new (self),
                                           (GDestroyNotify) foundry_weak_ref_free));

  if (priv->stream == NULL)
    {
      g_warning ("%s created without a stream, this cannot work!",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  priv->client = foundry_dap_client_new (priv->stream);

  g_signal_connect_object (priv->client,
                           "event",
                           G_CALLBACK (foundry_dap_debugger_client_event_cb),
                           self,
                           G_CONNECT_SWAPPED);

  foundry_dap_client_start (priv->client);
}

static void
foundry_dap_debugger_dispose (GObject *object)
{
  FoundryDapDebugger *self = (FoundryDapDebugger *)object;
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  if (priv->subprocess != NULL)
    g_subprocess_force_exit (priv->subprocess);

  if (priv->stream != NULL)
    g_io_stream_close (priv->stream, NULL, NULL);

  g_clear_object (&priv->stream);
  g_clear_object (&priv->subprocess);

  G_OBJECT_CLASS (foundry_dap_debugger_parent_class)->dispose (object);
}

static void
foundry_dap_debugger_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryDapDebugger *self = FOUNDRY_DAP_DEBUGGER (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      g_value_take_object (value, foundry_dap_debugger_dup_stream (self));
      break;

    case PROP_SUBPROCESS:
      g_value_take_object (value, foundry_dap_debugger_dup_subprocess (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_dap_debugger_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  FoundryDapDebugger *self = FOUNDRY_DAP_DEBUGGER (object);
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_STREAM:
      priv->stream = g_value_dup_object (value);
      break;

    case PROP_SUBPROCESS:
      priv->subprocess = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_dap_debugger_class_init (FoundryDapDebuggerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_dap_debugger_constructed;
  object_class->dispose = foundry_dap_debugger_dispose;
  object_class->get_property = foundry_dap_debugger_get_property;
  object_class->set_property = foundry_dap_debugger_set_property;

  properties[PROP_STREAM] =
    g_param_spec_object ("stream", NULL, NULL,
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBPROCESS] =
    g_param_spec_object ("subprocess", NULL, NULL,
                         G_TYPE_SUBPROCESS,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_dap_debugger_init (FoundryDapDebugger *self)
{
}

/**
 * foundry_dap_debugger_dup_subprocess:
 * @self: a [class@Foundry.DapDebugger]
 *
 * Returns: (transfer full) (nullable):
 */
GSubprocess *
foundry_dap_debugger_dup_subprocess (FoundryDapDebugger *self)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DAP_DEBUGGER (self), NULL);

  if (priv->subprocess)
    return g_object_ref (priv->subprocess);

  return NULL;
}

/**
 * foundry_dap_debugger_dup_stream:
 * @self: a [class@Foundry.DapDebugger]
 *
 * Returns: (transfer full) (nullable):
 */
GIOStream *
foundry_dap_debugger_dup_stream (FoundryDapDebugger *self)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DAP_DEBUGGER (self), NULL);

  if (priv->stream)
    return g_object_ref (priv->stream);

  return NULL;
}

/**
 * foundry_dap_debugger_dup_client:
 * @self: a [class@Foundry.DapDebugger]
 *
 * Returns: (transfer full):
 */
FoundryDapClient *
foundry_dap_debugger_dup_client (FoundryDapDebugger *self)
{
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_DAP_DEBUGGER (self), NULL);

  return g_object_ref (priv->client);
}
