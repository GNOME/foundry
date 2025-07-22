/* foundry-source-buffer-addin.c
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

#include "foundry-source-buffer-addin-private.h"

typedef struct
{
  FoundrySourceBuffer *buffer;
} FoundrySourceBufferAddinPrivate;

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundrySourceBufferAddin, foundry_source_buffer_addin, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
foundry_source_buffer_addin_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  FoundrySourceBufferAddin *self = FOUNDRY_SOURCE_BUFFER_ADDIN (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, foundry_source_buffer_addin_get_buffer (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_buffer_addin_class_init (FoundrySourceBufferAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_source_buffer_addin_get_property;

  properties[PROP_BUFFER] =
    g_param_spec_object ("buffer", NULL, NULL,
                         FOUNDRY_TYPE_SOURCE_BUFFER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_source_buffer_addin_init (FoundrySourceBufferAddin *self)
{
}

DexFuture *
foundry_source_buffer_addin_load (FoundrySourceBufferAddin *self,
                                  FoundrySourceBuffer      *buffer)
{
  FoundrySourceBufferAddinPrivate *priv = foundry_source_buffer_addin_get_instance_private (self);

  dex_return_error_if_fail (FOUNDRY_IS_SOURCE_BUFFER_ADDIN (self));
  dex_return_error_if_fail (FOUNDRY_IS_SOURCE_BUFFER (buffer));

  priv->buffer = buffer;

  if (FOUNDRY_SOURCE_BUFFER_ADDIN_GET_CLASS (self)->load)
    return FOUNDRY_SOURCE_BUFFER_ADDIN_GET_CLASS (self)->load (self);

  return dex_future_new_true ();
}

DexFuture *
foundry_source_buffer_addin_unload (FoundrySourceBufferAddin *self)
{
  FoundrySourceBufferAddinPrivate *priv = foundry_source_buffer_addin_get_instance_private (self);
  DexFuture *future;

  dex_return_error_if_fail (FOUNDRY_IS_SOURCE_BUFFER_ADDIN (self));

  if (FOUNDRY_SOURCE_BUFFER_ADDIN_GET_CLASS (self)->load)
    future = FOUNDRY_SOURCE_BUFFER_ADDIN_GET_CLASS (self)->load (self);
  else
    future = dex_future_new_true ();

  priv->buffer = NULL;

  return g_steal_pointer (&future);
}

/**
 * foundry_source_buffer_addin_get_buffer:
 * @self: a [class@Foundry.SourceBufferAddin]
 *
 * Returns: (transfer none) (nullable):
 */
FoundrySourceBuffer *
foundry_source_buffer_addin_get_buffer (FoundrySourceBufferAddin *self)
{
  FoundrySourceBufferAddinPrivate *priv = foundry_source_buffer_addin_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_SOURCE_BUFFER_ADDIN (self), NULL);

  return priv->buffer;
}
