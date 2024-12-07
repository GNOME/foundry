/* foundry-vcs-provider.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>

#include "foundry-vcs-provider-private.h"
#include "foundry-config-private.h"

G_DEFINE_ABSTRACT_TYPE (FoundryVcsProvider, foundry_vcs_provider, FOUNDRY_TYPE_CONTEXTUAL)

static DexFuture *
foundry_vcs_provider_real_load (FoundryVcsProvider *self)
{
  return dex_future_new_true ();
}

static DexFuture *
foundry_vcs_provider_real_unload (FoundryVcsProvider *self)
{
  return dex_future_new_true ();
}

static void
foundry_vcs_provider_class_init (FoundryVcsProviderClass *klass)
{
  klass->load = foundry_vcs_provider_real_load;
  klass->unload = foundry_vcs_provider_real_unload;
}

static void
foundry_vcs_provider_init (FoundryVcsProvider *self)
{
}

/**
 * foundry_vcs_provider_load:
 * @self: a #FoundryVcsProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_vcs_provider_load (FoundryVcsProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_PROVIDER (self), NULL);

  return FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->load (self);
}

/**
 * foundry_vcs_provider_unload:
 * @self: a #FoundryVcsProvider
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
foundry_vcs_provider_unload (FoundryVcsProvider *self)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_PROVIDER (self), NULL);

  return FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->unload (self);
}

/**
 * foundry_vcs_provider_dup_name:
 * @self: a #FoundryVcsProvider
 *
 * Gets a name for the provider that is expected to be displayed to
 * users such as "Flatpak".
 *
 * Returns: (transfer full): the name of the provider
 */
char *
foundry_vcs_provider_dup_name (FoundryVcsProvider *self)
{
  char *ret = NULL;

  g_return_val_if_fail (FOUNDRY_IS_VCS_PROVIDER (self), NULL);

  if (FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->dup_name)
    ret = FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->dup_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return g_steal_pointer (&ret);
}

/**
 * foundry_vcs_provider_supports_uri:
 * @self: a #FoundryVcsProvider
 *
 * Checks if a URI is supported by the VCS provider.
 *
 * This is useful to determine if you can get a downloader for a URI
 * to clone the repository.
 *
 * Returns: `true` if the URI is supported
 */
gboolean
foundry_vcs_provider_supports_uri (FoundryVcsProvider *self,
                                   const char         *uri_string)
{
  g_return_val_if_fail (FOUNDRY_IS_VCS_PROVIDER (self), FALSE);

  if (FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->supports_uri == NULL)
    return FALSE;

  return FOUNDRY_VCS_PROVIDER_GET_CLASS (self)->supports_uri (self, uri_string);
}
