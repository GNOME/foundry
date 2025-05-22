/*
 * foundry-source-buffer.c
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <libspelling.h>

#include "foundry-source-buffer.h"

struct _FoundrySourceBuffer
{
  GtkSourceBuffer            parent_instance;
  SpellingTextBufferAdapter *spelling_adapter;
  FoundryContext            *context;
  guint64                    change_count;
  guint                      enable_spellcheck : 1;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_ENABLE_SPELLCHECK,
  N_PROPS
};

static void text_buffer_iface_init (FoundryTextBufferInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundrySourceBuffer, foundry_source_buffer, GTK_SOURCE_TYPE_BUFFER,
                               G_IMPLEMENT_INTERFACE (FOUNDRY_TYPE_TEXT_BUFFER, text_buffer_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_source_buffer_changed (GtkTextBuffer *buffer)
{
  FoundrySourceBuffer *self = (FoundrySourceBuffer *)buffer;

  g_assert (FOUNDRY_IS_SOURCE_BUFFER (self));

  self->change_count++;

  GTK_TEXT_BUFFER_CLASS (foundry_source_buffer_parent_class)->changed (buffer);
}

static void
foundry_source_buffer_constructed (GObject *object)
{
  FoundrySourceBuffer *self = (FoundrySourceBuffer *)object;

  G_OBJECT_CLASS (foundry_source_buffer_parent_class)->constructed (object);

  self->spelling_adapter = spelling_text_buffer_adapter_new (GTK_SOURCE_BUFFER (self),
                                                             spelling_checker_get_default ());
  g_object_bind_property (object, "enable-spellcheck",
                          self->spelling_adapter, "enabled",
                          G_BINDING_SYNC_CREATE);
}

static void
foundry_source_buffer_dispose (GObject *object)
{
  FoundrySourceBuffer *self = (FoundrySourceBuffer *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->spelling_adapter);

  G_OBJECT_CLASS (foundry_source_buffer_parent_class)->dispose (object);
}

static void
foundry_source_buffer_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundrySourceBuffer *self = FOUNDRY_SOURCE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_ENABLE_SPELLCHECK:
      g_value_set_boolean (value, foundry_source_buffer_get_enable_spellcheck (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_buffer_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  FoundrySourceBuffer *self = FOUNDRY_SOURCE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    case PROP_ENABLE_SPELLCHECK:
      foundry_source_buffer_set_enable_spellcheck (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_buffer_class_init (FoundrySourceBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkTextBufferClass *text_buffer_class = GTK_TEXT_BUFFER_CLASS (klass);

  object_class->constructed = foundry_source_buffer_constructed;
  object_class->dispose = foundry_source_buffer_dispose;
  object_class->get_property = foundry_source_buffer_get_property;
  object_class->set_property = foundry_source_buffer_set_property;

  text_buffer_class->changed = foundry_source_buffer_changed;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         FOUNDRY_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ENABLE_SPELLCHECK] =
    g_param_spec_boolean ("enable-spellcheck", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_source_buffer_init (FoundrySourceBuffer *self)
{
  self->enable_spellcheck = TRUE;
}

/**
 * foundry_source_buffer_new:
 * @context: a [class@Foundry.Context]
 *
 * Returns: (transfer full): A new [class@Foundry.SourceBuffer]
 */
FoundrySourceBuffer *
foundry_source_buffer_new (FoundryContext *context)
{
  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  return g_object_new (FOUNDRY_TYPE_SOURCE_BUFFER,
                       "context", context,
                       NULL);
}

static GBytes *
foundry_source_buffer_dup_contents (FoundryTextBuffer *buffer)
{
  FoundrySourceBuffer *self = (FoundrySourceBuffer *)buffer;
  GtkTextIter begin, end;
  char *text;

  g_assert (FOUNDRY_IS_SOURCE_BUFFER (self));

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self), &begin, &end);
  text = gtk_text_iter_get_slice (&begin, &end);

  /* TODO: Special case when trailing newline is necessary */

  return g_bytes_new_take (text, strlen (text));
}

static gint64
foundry_source_buffer_get_change_count (FoundryTextBuffer *buffer)
{
  return FOUNDRY_SOURCE_BUFFER (buffer)->change_count;
}

static void
text_buffer_iface_init (FoundryTextBufferInterface *iface)
{
  iface->dup_contents = foundry_source_buffer_dup_contents;
  iface->get_change_count = foundry_source_buffer_get_change_count;
}

gboolean
foundry_source_buffer_get_enable_spellcheck (FoundrySourceBuffer *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_BUFFER (self), FALSE);

  return self->enable_spellcheck;
}

void
foundry_source_buffer_set_enable_spellcheck (FoundrySourceBuffer *self,
                                             gboolean             enable_spellcheck)
{
  g_return_if_fail (FOUNDRY_IS_SOURCE_BUFFER (self));

  enable_spellcheck = !!enable_spellcheck;

  if (self->enable_spellcheck != enable_spellcheck)
    {
      self->enable_spellcheck = enable_spellcheck;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLE_SPELLCHECK]);
    }
}
