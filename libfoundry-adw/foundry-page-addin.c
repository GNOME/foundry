/* foundry-page-addin.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "foundry-page-addin-private.h"

typedef struct
{
  GWeakRef page_wr;
} FoundryPageAddinPrivate;

/**
 * FoundryPageAddin:
 *
 * Extends a rooted [class@FoundryAdw.Page]. Addins are loaded after the page
 * is rooted and unloaded before it is unrooted, including when it moves
 * between windows. Implementations must release page widgets in unload.
 *
 * Since: 1.3
 */
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (FoundryPageAddin, foundry_page_addin, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_PAGE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_page_addin_real_load (FoundryPageAddin *self)
{
}

static void
foundry_page_addin_real_unload (FoundryPageAddin *self)
{
}

static void
foundry_page_addin_dispose (GObject *object)
{
  FoundryPageAddin *self = FOUNDRY_PAGE_ADDIN (object);
  FoundryPageAddinPrivate *priv = foundry_page_addin_get_instance_private (self);

  g_weak_ref_set (&priv->page_wr, NULL);

  G_OBJECT_CLASS (foundry_page_addin_parent_class)->dispose (object);
}

static void
foundry_page_addin_finalize (GObject *object)
{
  FoundryPageAddin *self = FOUNDRY_PAGE_ADDIN (object);
  FoundryPageAddinPrivate *priv = foundry_page_addin_get_instance_private (self);

  g_weak_ref_clear (&priv->page_wr);

  G_OBJECT_CLASS (foundry_page_addin_parent_class)->finalize (object);
}

static void
foundry_page_addin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryPageAddin *self = FOUNDRY_PAGE_ADDIN (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      g_value_take_object (value, foundry_page_addin_dup_page (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_page_addin_class_init (FoundryPageAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_page_addin_dispose;
  object_class->finalize = foundry_page_addin_finalize;
  object_class->get_property = foundry_page_addin_get_property;

  klass->load = foundry_page_addin_real_load;
  klass->unload = foundry_page_addin_real_unload;

  properties[PROP_PAGE] =
    g_param_spec_object ("page", NULL, NULL,
                         FOUNDRY_TYPE_PAGE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_page_addin_init (FoundryPageAddin *self)
{
  FoundryPageAddinPrivate *priv = foundry_page_addin_get_instance_private (self);

  g_weak_ref_init (&priv->page_wr, NULL);
}

/**
 * foundry_page_addin_dup_page:
 * @self: a [class@FoundryAdw.PageAddin]
 *
 * Gets the page currently using the addin.
 *
 * Returns: (transfer full) (nullable): the page while loaded
 * Since: 1.3
 */
FoundryPage *
foundry_page_addin_dup_page (FoundryPageAddin *self)
{
  FoundryPageAddinPrivate *priv = foundry_page_addin_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_PAGE_ADDIN (self), NULL);

  return g_weak_ref_get (&priv->page_wr);
}

void
_foundry_page_addin_load (FoundryPageAddin *self,
                          FoundryPage      *page)
{
  FoundryPageAddinPrivate *priv = foundry_page_addin_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_PAGE_ADDIN (self));
  g_return_if_fail (FOUNDRY_IS_PAGE (page));

  g_weak_ref_set (&priv->page_wr, page);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAGE]);

  FOUNDRY_PAGE_ADDIN_GET_CLASS (self)->load (self);
}

void
_foundry_page_addin_unload (FoundryPageAddin *self)
{
  FoundryPageAddinPrivate *priv = foundry_page_addin_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_PAGE_ADDIN (self));

  FOUNDRY_PAGE_ADDIN_GET_CLASS (self)->unload (self);
  g_weak_ref_set (&priv->page_wr, NULL);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAGE]);
}
