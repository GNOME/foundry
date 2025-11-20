/* foundry-llm-resource.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-llm-resource.h"
#include "foundry-util.h"

/**
 * FoundryLlmResource:
 *
 * Abstract base class for LLM resources.
 *
 * A resource represents external content that can be provided to an LLM,
 * such as files, URLs, or other data sources. Resources have metadata
 * including a URI, name, description, and content type.
 */
G_DEFINE_ABSTRACT_TYPE (FoundryLlmResource, foundry_llm_resource, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTENT_TYPE,
  PROP_DESCRIPTION,
  PROP_NAME,
  PROP_URI,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_llm_resource_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryLlmResource *self = FOUNDRY_LLM_RESOURCE (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_take_string (value, foundry_llm_resource_dup_uri (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, foundry_llm_resource_dup_name (self));
      break;

    case PROP_DESCRIPTION:
      g_value_take_string (value, foundry_llm_resource_dup_description (self));
      break;

    case PROP_CONTENT_TYPE:
      g_value_take_string (value, foundry_llm_resource_dup_content_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_llm_resource_class_init (FoundryLlmResourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_llm_resource_get_property;

  /**
   * FoundryLlmResource:uri:
   *
   * The URI of the resource.
   *
   * This identifies the location or source of the resource, which may
   * be a file path, URL, or other identifier depending on the resource type.
   */
  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryLlmResource:name:
   *
   * The name of the resource.
   *
   * A human-readable name for the resource, typically used for display
   * purposes in the UI.
   */
  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryLlmResource:description:
   *
   * The description of the resource.
   *
   * A human-readable description providing additional context about
   * the resource and its contents.
   */
  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundryLlmResource:content-type:
   *
   * The content type of the resource.
   *
   * This indicates the format of the resource data, such as "text/plain",
   * "application/json", or "image/png".
   */
  properties[PROP_CONTENT_TYPE] =
    g_param_spec_string ("content-type", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_llm_resource_init (FoundryLlmResource *self)
{
}

/**
 * foundry_llm_resource_dup_uri:
 * @self: a [class@Foundry.LlmResource]
 *
 * Gets the URI for the resource.
 *
 * Returns: (transfer full) (nullable): a newly allocated string containing
 *   the URI, or %NULL if not available
 *
 * Since: 1.1
 */
char *
foundry_llm_resource_dup_uri (FoundryLlmResource *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_RESOURCE (self), NULL);

  if (FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->dup_uri)
    return FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->dup_uri (self);

  return NULL;
}

/**
 * foundry_llm_resource_dup_name:
 * @self: a [class@Foundry.LlmResource]
 *
 * Gets the name for the resource.
 *
 * Returns: (transfer full) (nullable): a newly allocated string containing
 *   the name, or %NULL if not available
 *
 * Since: 1.1
 */
char *
foundry_llm_resource_dup_name (FoundryLlmResource *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_RESOURCE (self), NULL);

  if (FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->dup_name)
    return FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->dup_name (self);

  return NULL;
}

/**
 * foundry_llm_resource_dup_description:
 * @self: a [class@Foundry.LlmResource]
 *
 * Gets the description for the resource.
 *
 * Returns: (transfer full) (nullable): a newly allocated string containing
 *   the description, or %NULL if not available
 *
 * Since: 1.1
 */
char *
foundry_llm_resource_dup_description (FoundryLlmResource *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_RESOURCE (self), NULL);

  if (FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->dup_description)
    return FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->dup_description (self);

  return NULL;
}

/**
 * foundry_llm_resource_dup_content_type:
 * @self: a [class@Foundry.LlmResource]
 *
 * Gets the content type for the resource.
 *
 * Returns: (transfer full) (nullable): a newly allocated string containing
 *   the content type, or %NULL if not available
 *
 * Since: 1.1
 */
char *
foundry_llm_resource_dup_content_type (FoundryLlmResource *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_RESOURCE (self), NULL);

  if (FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->dup_content_type)
    return FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->dup_content_type (self);

  return NULL;
}

/**
 * foundry_llm_resource_load_bytes:
 * @self: a [class@Foundry.LlmResource]
 *
 * Loads the bytes for the resource.
 *
 * This method asynchronously loads the resource content and returns
 * a future that resolves to a [struct@GLib.Bytes] containing the data.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [struct@GLib.Bytes], or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_llm_resource_load_bytes (FoundryLlmResource *self)
{
  g_return_val_if_fail (FOUNDRY_IS_LLM_RESOURCE (self), NULL);

  if (FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->load_bytes)
    return FOUNDRY_LLM_RESOURCE_GET_CLASS (self)->load_bytes (self);

  return foundry_future_new_not_supported ();
}
