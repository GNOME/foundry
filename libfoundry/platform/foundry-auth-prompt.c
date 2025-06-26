/* foundry-auth-prompt.c
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

#include "foundry-auth-prompt.h"
#include "foundry-auth-provider.h"
#include "foundry-context.h"
#include "foundry-debug.h"
#include "foundry-extension.h"

typedef struct _Param
{
  char *id;
  char *name;
  char *value;
  guint hidden : 1;
} Param;

struct _FoundryAuthPrompt
{
  FoundryContextual parent_instance;
  GMutex mutex;
  char *title;
  char *subtitle;
  GArray *params;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryAuthPrompt, foundry_auth_prompt, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static void
param_clear (gpointer data)
{
  Param *p = data;

  g_clear_pointer (&p->id, g_free);
  g_clear_pointer (&p->name, g_free);
  g_clear_pointer (&p->value, g_free);
}

static void
foundry_auth_prompt_finalize (GObject *object)
{
  FoundryAuthPrompt *self = (FoundryAuthPrompt *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  g_clear_pointer (&self->params, g_array_unref);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (foundry_auth_prompt_parent_class)->finalize (object);
}

static void
foundry_auth_prompt_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryAuthPrompt *self = FOUNDRY_AUTH_PROMPT (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, self->subtitle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_auth_prompt_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundryAuthPrompt *self = FOUNDRY_AUTH_PROMPT (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      if (g_set_str (&self->title, g_value_get_string (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_SUBTITLE:
      if (g_set_str (&self->subtitle, g_value_get_string (value)))
        g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_auth_prompt_class_init (FoundryAuthPromptClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_auth_prompt_finalize;
  object_class->get_property = foundry_auth_prompt_get_property;
  object_class->set_property = foundry_auth_prompt_set_property;

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_auth_prompt_init (FoundryAuthPrompt *self)
{
  g_mutex_init (&self->mutex);
}

struct _FoundryAuthPromptBuilder
{
  FoundryContext *context;
  char *title;
  char *subtitle;
  GArray *params;
};

FoundryAuthPromptBuilder *
foundry_auth_prompt_builder_new (FoundryContext *context)
{
  FoundryAuthPromptBuilder *builder;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);

  builder = g_new0 (FoundryAuthPromptBuilder, 1);
  builder->context = g_object_ref (context);

  return builder;
}

/**
 * foundry_auth_prompt_builder_end:
 *
 * Returns: (transfer full):
 */
FoundryAuthPrompt *
foundry_auth_prompt_builder_end (FoundryAuthPromptBuilder *builder)
{
  FoundryAuthPrompt *self;

  g_return_val_if_fail (builder != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_AUTH_PROMPT,
                       "context", builder->context,
                       NULL);
  self->title = g_steal_pointer (&builder->title);
  self->subtitle = g_steal_pointer (&builder->subtitle);
  self->params = g_steal_pointer (&builder->params);

  return self;
}

void
foundry_auth_prompt_builder_free (FoundryAuthPromptBuilder *builder)
{
  g_clear_pointer (&builder->title, g_free);
  g_clear_pointer (&builder->subtitle, g_free);
  g_clear_pointer (&builder->params, g_array_unref);
  g_clear_object (&builder->context);
  g_free (builder);
}

static FoundryAuthPromptBuilder *
foundry_auth_prompt_builder_copy (FoundryAuthPromptBuilder *builder)
{
  FoundryAuthPromptBuilder *copy = g_new0 (FoundryAuthPromptBuilder, 1);

  copy->title = g_strdup (builder->title);
  copy->subtitle = g_strdup (builder->subtitle);

  if (builder->params != NULL)
    {
      for (guint i = 0; i < builder->params->len; i++)
        {
          const Param *p = &g_array_index (builder->params, Param, i);

          foundry_auth_prompt_builder_add_param (copy, p->id, p->name, p->value, p->hidden);
        }
    }

  return copy;
}

G_DEFINE_BOXED_TYPE (FoundryAuthPromptBuilder,
                     foundry_auth_prompt_builder,
                     (GBoxedCopyFunc) foundry_auth_prompt_builder_copy,
                     (GBoxedFreeFunc) foundry_auth_prompt_builder_free)

void
foundry_auth_prompt_builder_set_title (FoundryAuthPromptBuilder *builder,
                                       const char               *title)
{
  g_return_if_fail (builder != NULL);

  g_set_str (&builder->title, title);
}

void
foundry_auth_prompt_builder_set_subtitle (FoundryAuthPromptBuilder *builder,
                                          const char               *subtitle)
{
  g_return_if_fail (builder != NULL);

  g_set_str (&builder->subtitle, subtitle);
}

/**
 * foundry_auth_prompt_builder_add_param:
 * @builder: a [class@Foundry.AuthPromptBuilder]
 * @id: the identifier for the param like "username"
 * @name: the translated name for the param like _("Username")
 * @value: (nullable): the initial value for the param
 * @hidden: if the param input should be hidden or obscured such as that
 *   of a password entry.
 */
void
foundry_auth_prompt_builder_add_param (FoundryAuthPromptBuilder *builder,
                                       const char               *id,
                                       const char               *name,
                                       const char               *value,
                                       gboolean                  hidden)
{
  Param p;

  g_return_if_fail (builder != NULL);
  g_return_if_fail (id != NULL);
  g_return_if_fail (name != NULL);

  if (builder->params == NULL)
    {
      builder->params = g_array_new (FALSE, FALSE, sizeof (Param));
      g_array_set_clear_func (builder->params, (GDestroyNotify)param_clear);
    }

  p.id = g_strdup (id);
  p.name = g_strdup (name);
  p.value = g_strdup (value);
  p.hidden = !!hidden;

  g_array_append_val (builder->params, p);
}

const char *
foundry_auth_prompt_get_value (FoundryAuthPrompt *self,
                               const char        *id)
{
  g_return_val_if_fail (FOUNDRY_IS_AUTH_PROMPT (self), NULL);

  if (self->params == NULL)
    return NULL;

  for (guint i = 0; i < self->params->len; i++)
    {
      const Param *p = &g_array_index (self->params, Param, i);

      if (g_strcmp0 (id, p->id) == 0)
        return p->value;
    }

  g_warning ("No such parameter `%s`", id);

  return NULL;
}

static DexFuture *
foundry_auth_prompt_query_fiber (gpointer data)
{
  FoundryAuthPrompt *self = data;
  g_autoptr(FoundryExtension) adapter = NULL;
  g_autoptr(FoundryContext) context = NULL;
  FoundryAuthProvider *provider;

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_AUTH_PROMPT (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  adapter = foundry_extension_new (context,
                                   peas_engine_get_default (),
                                   FOUNDRY_TYPE_AUTH_PROVIDER,
                                   "Auth-Provider", "*");
  provider = foundry_extension_get_extension (adapter);

  if (provider == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Not supported");

  return foundry_auth_provider_prompt (provider, self);
}

/**
 * foundry_auth_prompt_query:
 * @self: a [class@Foundry.AuthPrompt]
 *
 * Wait for the user to populate auth information.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   if the auth prompt was successfully completed by the user.
 */
DexFuture *
foundry_auth_prompt_query (FoundryAuthPrompt *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_AUTH_PROMPT (self));

  return dex_scheduler_spawn (dex_scheduler_get_default (), 0,
                              foundry_auth_prompt_query_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

/**
 * foundry_auth_prompt_dup_prompts:
 * @self: a [class@Foundry.AuthPrompt]
 *
 * Get the identifiers of the propmts.
 *
 * Returns: (transfer full) (nullable):
 */
char **
foundry_auth_prompt_dup_prompts (FoundryAuthPrompt *self)
{
  g_autoptr(GStrvBuilder) builder = NULL;

  g_return_val_if_fail (FOUNDRY_IS_AUTH_PROMPT (self), NULL);

  builder = g_strv_builder_new ();

  if (self->params != NULL)
    {
      for (guint i = 0; i < self->params->len; i++)
        {
          const Param *p = &g_array_index (self->params, Param, i);

          g_strv_builder_add (builder, p->id);
        }
    }

  return g_strv_builder_end (builder);
}

char *
foundry_auth_prompt_dup_title (FoundryAuthPrompt *self)
{
  g_return_val_if_fail (FOUNDRY_IS_AUTH_PROMPT (self), NULL);

  return g_strdup (self->title);
}

char *
foundry_auth_prompt_dup_subtitle (FoundryAuthPrompt *self)
{
  g_return_val_if_fail (FOUNDRY_IS_AUTH_PROMPT (self), NULL);

  return g_strdup (self->subtitle);
}

static Param *
get_param (FoundryAuthPrompt *self,
           const char        *id)
{
  g_return_val_if_fail (FOUNDRY_IS_AUTH_PROMPT (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  if (self->params == NULL)
    return NULL;

  for (guint i = 0; i < self->params->len; i++)
    {
      Param *p = &g_array_index (self->params, Param, i);

      if (g_strcmp0 (p->id, id) == 0)
        return p;
    }

  return NULL;
}

char *
foundry_auth_prompt_dup_prompt_name (FoundryAuthPrompt *self,
                                     const char        *id)
{
  Param *p = get_param (self, id);

  return p ? g_strdup (p->name) : NULL;
}

char *
foundry_auth_prompt_dup_prompt_value (FoundryAuthPrompt *self,
                                      const char        *id)
{
  Param *p = get_param (self, id);
  char *ret;

  g_mutex_lock (&self->mutex);
  ret = p ? g_strdup (p->value) : NULL;
  g_mutex_unlock (&self->mutex);

  return ret;
}

void
foundry_auth_prompt_set_prompt_value (FoundryAuthPrompt *self,
                                      const char        *id,
                                      const char        *value)
{
  Param *p;

  g_return_if_fail (FOUNDRY_IS_AUTH_PROMPT (self));
  g_return_if_fail (id != NULL);

  if (!(p = get_param (self, id)))
    return;

  g_mutex_lock (&self->mutex);
  g_set_str (&p->value, value);
  g_mutex_unlock (&self->mutex);
}

gboolean
foundry_auth_prompt_is_prompt_hidden (FoundryAuthPrompt *self,
                                      const char        *id)
{
  Param *p = get_param (self, id);

  return p ? p->hidden : FALSE;
}
