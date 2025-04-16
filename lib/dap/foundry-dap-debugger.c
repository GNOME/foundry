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

#include "foundry-dap-debugger.h"

typedef struct
{
  GIOStream   *stream;
  GSubprocess *subprocess;
} FoundryDapDebuggerPrivate;

enum {
  PROP_0,
  PROP_STREAM,
  PROP_SUBPROCESS,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (FoundryDapDebugger, foundry_dap_debugger, FOUNDRY_TYPE_DEBUGGER)

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_dap_debugger_exited (DexFuture *future,
                             gpointer   user_data)
{
  FoundryDapDebugger *self = user_data;
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);
  g_autoptr(GError) error = NULL;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (FOUNDRY_IS_DAP_DEBUGGER (self));

  if (!dex_await (dex_ref (future), &error))
    g_io_stream_close (priv->stream, NULL, NULL);

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
                                           g_object_ref (self),
                                           g_object_unref));
}

static void
foundry_dap_debugger_dispose (GObject *object)
{
  FoundryDapDebugger *self = (FoundryDapDebugger *)object;
  FoundryDapDebuggerPrivate *priv = foundry_dap_debugger_get_instance_private (self);

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

FoundryDebugger *
foundry_dap_debugger_new (FoundryContext *context,
                          GSubprocess    *subprocess,
                          GIOStream      *stream)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_SUBPROCESS (subprocess), NULL);
  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);

  return g_object_new (FOUNDRY_TYPE_DAP_DEBUGGER,
                       "context", context,
                       "subprocess", subprocess,
                       "stream", stream,
                       NULL);
}
