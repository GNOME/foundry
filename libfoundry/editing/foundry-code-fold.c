/* foundry-code-fold.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-code-fold.h"

/**
 * FoundryCodeFold:
 *
 * An immutable fold with a visible anchor and hidden character interval.
 * The interval is [start, end) in a captured text buffer snapshot.
 *
 * Since: 1.3
 */

struct _FoundryCodeFold
{
  GObject parent_instance;
  guint anchor;
  guint start;
  guint end;
};

enum {
  PROP_0,
  PROP_ANCHOR,
  PROP_START,
  PROP_END,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryCodeFold, foundry_code_fold, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_code_fold_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  FoundryCodeFold *self = FOUNDRY_CODE_FOLD (object);

  switch (prop_id)
    {
    case PROP_ANCHOR:
      g_value_set_uint (value, self->anchor);
      break;

    case PROP_START:
      g_value_set_uint (value, self->start);
      break;

    case PROP_END:
      g_value_set_uint (value, self->end);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_code_fold_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  FoundryCodeFold *self = FOUNDRY_CODE_FOLD (object);

  switch (prop_id)
    {
    case PROP_ANCHOR:
      self->anchor = g_value_get_uint (value);
      break;

    case PROP_START:
      self->start = g_value_get_uint (value);
      break;

    case PROP_END:
      self->end = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_code_fold_class_init (FoundryCodeFoldClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_code_fold_get_property;
  object_class->set_property = foundry_code_fold_set_property;

  properties[PROP_ANCHOR] =
    g_param_spec_uint ("anchor", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_START] =
    g_param_spec_uint ("start", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS));

  properties[PROP_END] =
    g_param_spec_uint ("end", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_code_fold_init (FoundryCodeFold *self)
{
}

/**
 * foundry_code_fold_new:
 * @anchor: visible control offset
 * @start: first hidden character offset
 * @end: exclusive hidden end offset
 *
 * Returns: (transfer full) (nullable): a valid fold
 * Since: 1.3
 */
FoundryCodeFold *
foundry_code_fold_new (guint anchor,
                       guint start,
                       guint end)
{
  g_return_val_if_fail (anchor <= start, NULL);
  g_return_val_if_fail (start < end, NULL);

  return g_object_new (FOUNDRY_TYPE_CODE_FOLD,
                       "anchor", anchor,
                       "start", start,
                       "end", end,
                       NULL);
}

/**
 * foundry_code_fold_get_anchor:
 * @self: a code fold
 *
 * Returns: the visible anchor offset
 * Since: 1.3
 */
guint
foundry_code_fold_get_anchor (FoundryCodeFold *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CODE_FOLD (self), 0);

  return self->anchor;
}

/**
 * foundry_code_fold_get_start:
 * @self: a code fold
 *
 * Returns: the first hidden character offset
 * Since: 1.3
 */
guint
foundry_code_fold_get_start (FoundryCodeFold *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CODE_FOLD (self), 0);

  return self->start;
}

/**
 * foundry_code_fold_get_end:
 * @self: a code fold
 *
 * Returns: the exclusive hidden end offset
 * Since: 1.3
 */
guint
foundry_code_fold_get_end (FoundryCodeFold *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CODE_FOLD (self), 0);

  return self->end;
}
