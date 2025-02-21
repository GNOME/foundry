/* foundry-dependency.c
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

#include "foundry-dependency.h"

enum {
  PROP_0,
  PROP_KIND,
  PROP_NAME,
  PROP_LOCATION,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (FoundryDependency, foundry_dependency, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_dependency_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryDependency *self = FOUNDRY_DEPENDENCY (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_take_string (value, foundry_dependency_dup_name (self));
      break;

    case PROP_KIND:
      g_value_take_string (value, foundry_dependency_dup_kind (self));
      break;

    case PROP_LOCATION:
      g_value_take_string (value, foundry_dependency_dup_location (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_dependency_class_init (FoundryDependencyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_dependency_get_property;

  properties[PROP_KIND] =
    g_param_spec_string ("kind", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LOCATION] =
    g_param_spec_string ("location", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_dependency_init (FoundryDependency *self)
{
}

char *
foundry_dependency_dup_name (FoundryDependency *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEPENDENCY (self), NULL);

  if (FOUNDRY_DEPENDENCY_GET_CLASS (self)->dup_name)
    return FOUNDRY_DEPENDENCY_GET_CLASS (self)->dup_name (self);

  return NULL;
}

char *
foundry_dependency_dup_location (FoundryDependency *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEPENDENCY (self), NULL);

  if (FOUNDRY_DEPENDENCY_GET_CLASS (self)->dup_location)
    return FOUNDRY_DEPENDENCY_GET_CLASS (self)->dup_location (self);

  return NULL;
}

char *
foundry_dependency_dup_kind (FoundryDependency *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DEPENDENCY (self), NULL);

  if (FOUNDRY_DEPENDENCY_GET_CLASS (self)->dup_kind)
    return FOUNDRY_DEPENDENCY_GET_CLASS (self)->dup_kind (self);

  return NULL;
}

/**
 * foundry_dependency_update:
 * @self: a [class@Foundry.Dependency]
 * @cancellable: (nullable): a [class@Dex.Cancellable]
 * @pty_fd: a PTY for output
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   to any value or rejects with error.
 */
DexFuture *
foundry_dependency_update (FoundryDependency *self,
                           DexCancellable    *cancellable,
                           int                pty_fd)
{
  dex_return_error_if_fail (FOUNDRY_IS_DEPENDENCY (self));
  dex_return_error_if_fail (!cancellable || DEX_IS_CANCELLABLE (cancellable));
  dex_return_error_if_fail (pty_fd >= -1);

  if (FOUNDRY_DEPENDENCY_GET_CLASS (self)->update)
    return FOUNDRY_DEPENDENCY_GET_CLASS (self)->update (self, cancellable, pty_fd);

  return dex_future_new_true ();
}
