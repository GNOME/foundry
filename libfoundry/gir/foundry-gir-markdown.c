/* foundry-gir-markdown.c
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

#include <tmpl-glib.h>

#include "foundry-gir-markdown.h"

struct _FoundryGirMarkdown
{
  GObject         parent_instance;
  FoundryGir     *gir;
  FoundryGirNode *node;
};

enum {
  PROP_0,
  PROP_GIR,
  PROP_NODE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryGirMarkdown, foundry_gir_markdown, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_gir_markdown_finalize (GObject *object)
{
  FoundryGirMarkdown *self = (FoundryGirMarkdown *)object;

  g_clear_object (&self->gir);
  g_clear_object (&self->node);

  G_OBJECT_CLASS (foundry_gir_markdown_parent_class)->finalize (object);
}

static void
foundry_gir_markdown_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryGirMarkdown *self = FOUNDRY_GIR_MARKDOWN (object);

  switch (prop_id)
    {
    case PROP_GIR:
      g_value_set_object (value, foundry_gir_markdown_get_gir (self));
      break;

    case PROP_NODE:
      g_value_set_object (value, foundry_gir_markdown_get_node (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_gir_markdown_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  FoundryGirMarkdown *self = FOUNDRY_GIR_MARKDOWN (object);

  switch (prop_id)
    {
    case PROP_GIR:
      self->gir = g_value_dup_object (value);
      break;

    case PROP_NODE:
      foundry_gir_markdown_set_node (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_gir_markdown_class_init (FoundryGirMarkdownClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_gir_markdown_finalize;
  object_class->get_property = foundry_gir_markdown_get_property;
  object_class->set_property = foundry_gir_markdown_set_property;

  properties[PROP_GIR] =
    g_param_spec_object ("gir", NULL, NULL,
                         FOUNDRY_TYPE_GIR,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NODE] =
    g_param_spec_object ("node", NULL, NULL,
                         FOUNDRY_TYPE_GIR_NODE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_gir_markdown_init (FoundryGirMarkdown *self)
{
}

FoundryGirMarkdown *
foundry_gir_markdown_new (FoundryGir *gir)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR (gir), NULL);

  return g_object_new (FOUNDRY_TYPE_GIR_MARKDOWN,
                       "gir", gir,
                       NULL);
}

/**
 * foundry_gir_markdown_get_gir:
 * @self: a [class@Foundry.GirMarkdown]
 *
 * Returns: (transfer none) (not nullable):
 *
 * Since: 1.1
 */
FoundryGir *
foundry_gir_markdown_get_gir (FoundryGirMarkdown *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_MARKDOWN (self), NULL);

  return self->gir;
}

/**
 * foundry_gir_markdown_get_node:
 * @self: a [class@Foundry.GirMarkdown]
 *
 * Returns: (transfer none) (nullable):
 *
 * Since: 1.1
 */
FoundryGirNode *
foundry_gir_markdown_get_node (FoundryGirMarkdown *self)
{
  g_return_val_if_fail (FOUNDRY_IS_GIR_MARKDOWN (self), NULL);

  return self->node;
}

void
foundry_gir_markdown_set_node (FoundryGirMarkdown *self,
                               FoundryGirNode     *node)
{
  g_return_if_fail (FOUNDRY_IS_GIR_MARKDOWN (self));
  g_return_if_fail (FOUNDRY_IS_GIR_NODE (node));

  if (g_set_object (&self->node, node))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NODE]);
}

char *
foundry_gir_markdown_generate (FoundryGirMarkdown  *self,
                               GError             **error)
{
  g_autoptr(TmplTemplateLocator) locator = NULL;
  g_autoptr(TmplTemplate) template = NULL;
  g_autoptr(TmplScope) scope = NULL;
  FoundryGirNode *node;

  g_return_val_if_fail (FOUNDRY_IS_GIR_MARKDOWN (self), NULL);

  locator = tmpl_template_locator_new ();
  template = tmpl_template_new (locator);
  scope = tmpl_scope_new ();

  tmpl_template_locator_append_search_path (locator, "resource:///app/devsuite/foundry/gir/md/");
  if (!tmpl_template_parse_resource (template, "/app/devsuite/foundry/gir/md/node.tmpl", NULL, error))
    return NULL;

  if (!(node = self->node))
    node = foundry_gir_get_repository (self->gir);

  tmpl_scope_set_object (scope, "gir", G_OBJECT (self->gir));
  tmpl_scope_set_object (scope, "node", G_OBJECT (node));

  return tmpl_template_expand_string (template, scope, error);
}
