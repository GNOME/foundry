/* foundry-documentation-intent.c
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

#include "foundry-documentation-intent.h"

/**
 * FoundryDocumentationIntent:
 *
 * Represents an intent to display documentation.
 *
 * FoundryDocumentationIntent provides functionality for displaying
 * a [class@Foundry.Documentation] object. It integrates with the intent
 * system to provide a unified interface for documentation display
 * operations across different parts of the development environment.
 *
 * Since: 1.1
 */

struct _FoundryDocumentationIntent
{
  FoundryIntent         parent_instance;
  FoundryDocumentation *documentation;
};

enum {
  PROP_0,
  PROP_DOCUMENTATION,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryDocumentationIntent, foundry_documentation_intent, FOUNDRY_TYPE_INTENT)

static GParamSpec *properties[N_PROPS];

static void
foundry_documentation_intent_finalize (GObject *object)
{
  FoundryDocumentationIntent *self = FOUNDRY_DOCUMENTATION_INTENT (object);

  g_clear_object (&self->documentation);

  G_OBJECT_CLASS (foundry_documentation_intent_parent_class)->finalize (object);
}

static void
foundry_documentation_intent_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  FoundryDocumentationIntent *self = FOUNDRY_DOCUMENTATION_INTENT (object);

  switch (prop_id)
    {
    case PROP_DOCUMENTATION:
      g_value_take_object (value, foundry_documentation_intent_dup_documentation (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_documentation_intent_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  FoundryDocumentationIntent *self = FOUNDRY_DOCUMENTATION_INTENT (object);

  switch (prop_id)
    {
    case PROP_DOCUMENTATION:
      self->documentation = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_documentation_intent_class_init (FoundryDocumentationIntentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_documentation_intent_finalize;
  object_class->get_property = foundry_documentation_intent_get_property;
  object_class->set_property = foundry_documentation_intent_set_property;

  properties[PROP_DOCUMENTATION] =
    g_param_spec_object ("documentation", NULL, NULL,
                         FOUNDRY_TYPE_DOCUMENTATION,
                         (G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_documentation_intent_init (FoundryDocumentationIntent *self)
{
}

/**
 * foundry_documentation_intent_new:
 * @documentation: a [class@Foundry.Documentation]
 *
 * Creates a new [class@Foundry.DocumentationIntent] for displaying
 * the given @documentation.
 *
 * Returns: (transfer full): a new [class@Foundry.DocumentationIntent]
 *
 * Since: 1.1
 */
FoundryIntent *
foundry_documentation_intent_new (FoundryDocumentation *documentation)
{
  g_return_val_if_fail (FOUNDRY_IS_DOCUMENTATION (documentation), NULL);

  return g_object_new (FOUNDRY_TYPE_DOCUMENTATION_INTENT,
                       "documentation", documentation,
                       NULL);
}

/**
 * foundry_documentation_intent_dup_documentation:
 * @self: a [class@Foundry.DocumentationIntent]
 *
 * Gets a copy of the documentation object.
 *
 * Returns: (transfer full) (not nullable): a [class@Foundry.Documentation]
 *
 * Since: 1.1
 */
FoundryDocumentation *
foundry_documentation_intent_dup_documentation (FoundryDocumentationIntent *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DOCUMENTATION_INTENT (self), NULL);

  return g_object_ref (self->documentation);
}
