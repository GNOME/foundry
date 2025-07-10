/* foundry-llm-completion-params.c
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

#include "foundry-llm-completion-params.h"

struct _FoundryLlmCompletionParams
{
  GObject parent_instance;
  char *prompt;
  char *suffix;
  char *system;
  char *context;
  guint raw : 1;
};

enum {
  PROP_0,
  PROP_PROMPT,
  PROP_SUFFIX,
  PROP_SYSTEM,
  PROP_CONTEXT,
  PROP_RAW,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryLlmCompletionParams, foundry_llm_completion_params, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_llm_completion_params_finalize (GObject *object)
{
  FoundryLlmCompletionParams *self = (FoundryLlmCompletionParams *)object;

  g_clear_pointer (&self->prompt, g_free);
  g_clear_pointer (&self->suffix, g_free);
  g_clear_pointer (&self->system, g_free);
  g_clear_pointer (&self->context, g_free);

  G_OBJECT_CLASS (foundry_llm_completion_params_parent_class)->finalize (object);
}

static void
foundry_llm_completion_params_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  FoundryLlmCompletionParams *self = FOUNDRY_LLM_COMPLETION_PARAMS (object);

  switch (prop_id)
    {
    case PROP_PROMPT:
      g_value_take_string (value, foundry_llm_completion_params_dup_prompt (self));
      break;

    case PROP_SUFFIX:
      g_value_take_string (value, foundry_llm_completion_params_dup_suffix (self));
      break;

    case PROP_SYSTEM:
      g_value_take_string (value, foundry_llm_completion_params_dup_system (self));
      break;

    case PROP_CONTEXT:
      g_value_take_string (value, foundry_llm_completion_params_dup_context (self));
      break;

    case PROP_RAW:
      g_value_set_boolean (value, foundry_llm_completion_params_get_raw (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_completion_params_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  FoundryLlmCompletionParams *self = FOUNDRY_LLM_COMPLETION_PARAMS (object);

  switch (prop_id)
    {
    case PROP_PROMPT:
      foundry_llm_completion_params_set_prompt (self, g_value_get_string (value));
      break;

    case PROP_SUFFIX:
      foundry_llm_completion_params_set_suffix (self, g_value_get_string (value));
      break;

    case PROP_SYSTEM:
      foundry_llm_completion_params_set_system (self, g_value_get_string (value));
      break;

    case PROP_CONTEXT:
      foundry_llm_completion_params_set_context (self, g_value_get_string (value));
      break;

    case PROP_RAW:
      foundry_llm_completion_params_set_raw (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_completion_params_class_init (FoundryLlmCompletionParamsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_llm_completion_params_finalize;
  object_class->get_property = foundry_llm_completion_params_get_property;
  object_class->set_property = foundry_llm_completion_params_set_property;

  properties[PROP_PROMPT] =
    g_param_spec_string ("prompt", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SUFFIX] =
    g_param_spec_string ("suffix", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SYSTEM] =
    g_param_spec_string ("system", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_CONTEXT] =
    g_param_spec_string ("context", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_RAW] =
    g_param_spec_boolean ("raw", NULL, NULL,
                         FALSE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_llm_completion_params_init (FoundryLlmCompletionParams *self)
{
}

char *
foundry_llm_completion_params_dup_prompt (FoundryLlmCompletionParams *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self), NULL);

  return g_strdup (self->prompt);
}

void
foundry_llm_completion_params_set_prompt (FoundryLlmCompletionParams *self,
                                          const char                 *prompt)
{
  g_return_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self));

  if (g_set_str (&self->prompt, prompt))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROMPT]);
}

char *
foundry_llm_completion_params_dup_suffix (FoundryLlmCompletionParams *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self), NULL);

  return g_strdup (self->suffix);
}

void
foundry_llm_completion_params_set_suffix (FoundryLlmCompletionParams *self,
                                          const char                 *suffix)
{
  g_return_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self));

  if (g_set_str (&self->suffix, suffix))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUFFIX]);
}

char *
foundry_llm_completion_params_dup_system (FoundryLlmCompletionParams *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self), NULL);

  return g_strdup (self->system);
}

void
foundry_llm_completion_params_set_system (FoundryLlmCompletionParams *self,
                                          const char                 *system)
{
  g_return_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self));

  if (g_set_str (&self->system, system))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SYSTEM]);
}

char *
foundry_llm_completion_params_dup_context (FoundryLlmCompletionParams *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self), NULL);

  return g_strdup (self->context);
}

void
foundry_llm_completion_params_set_context (FoundryLlmCompletionParams *self,
                                           const char                 *context)
{
  g_return_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self));

  if (g_set_str (&self->context, context))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTEXT]);
}

gboolean
foundry_llm_completion_params_get_raw (FoundryLlmCompletionParams *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self), FALSE);

  return self->raw;
}

void
foundry_llm_completion_params_set_raw (FoundryLlmCompletionParams *self,
                                       gboolean                    raw)
{
  g_return_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self));

  raw = !!raw;

  if (self->raw != raw)
    {
      self->raw = raw;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RAW]);
    }
}
