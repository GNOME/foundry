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

typedef struct
{
  char *prompt;
} FoundryLlmCompletionParamsPrivate;

enum {
  PROP_0,
  PROP_PROMPT,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (FoundryLlmCompletionParams, foundry_llm_completion_params, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_llm_completion_params_finalize (GObject *object)
{
  FoundryLlmCompletionParams *self = (FoundryLlmCompletionParams *)object;
  FoundryLlmCompletionParamsPrivate *priv = foundry_llm_completion_params_get_instance_private (self);

  g_clear_pointer (&priv->prompt, g_free);

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

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_llm_completion_params_init (FoundryLlmCompletionParams *self)
{
}

char *
foundry_llm_completion_params_dup_prompt (FoundryLlmCompletionParams *self)
{
  FoundryLlmCompletionParamsPrivate *priv = foundry_llm_completion_params_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self), NULL);

  return g_strdup (priv->prompt);
}

void
foundry_llm_completion_params_set_prompt (FoundryLlmCompletionParams *self,
                                          const char                 *prompt)
{
  FoundryLlmCompletionParamsPrivate *priv = foundry_llm_completion_params_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_LLM_COMPLETION_PARAMS (self));

  if (g_set_str (&priv->prompt, prompt))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROMPT]);
}

FoundryLlmCompletionParams *
foundry_llm_completion_params_new (void)
{
  return g_object_new (FOUNDRY_TYPE_LLM_COMPLETION_PARAMS, NULL);
}
