/* foundry-key-rotator.c
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

#include "foundry-key-rotator.h"
#include "foundry-util.h"

/**
 * FoundryKeyRotator:
 *
 * Abstract base class for rotating API keys.
 *
 * FoundryKeyRotator provides an interface for rotating API keys
 * associated with a host and service name. Subclasses implement
 * the actual rotation logic for specific services or providers.
 *
 * Since: 1.1
 */

G_DEFINE_ABSTRACT_TYPE (FoundryKeyRotator, foundry_key_rotator, FOUNDRY_TYPE_CONTEXTUAL)

static gboolean
foundry_key_rotator_real_can_rotate (FoundryKeyRotator *self,
                                     const char        *host,
                                     const char        *service_name,
                                     const char        *secret)
{
  return FALSE;
}

static DexFuture *
foundry_key_rotator_real_rotate (FoundryKeyRotator *self,
                                 const char        *host,
                                 const char        *service_name,
                                 const char        *secret,
                                 GDateTime         *expire_at)
{
  return foundry_future_new_not_supported ();
}

static DexFuture *
foundry_key_rotator_real_check_expires_at (FoundryKeyRotator *self,
                                           const char        *host,
                                           const char        *service_name,
                                           const char        *secret)
{
  return foundry_future_new_not_supported ();
}

static void
foundry_key_rotator_class_init (FoundryKeyRotatorClass *klass)
{
  klass->check_expires_at = foundry_key_rotator_real_check_expires_at;
  klass->can_rotate = foundry_key_rotator_real_can_rotate;
  klass->rotate = foundry_key_rotator_real_rotate;
}

static void
foundry_key_rotator_init (FoundryKeyRotator *self)
{
}

/**
 * foundry_key_rotator_can_rotate:
 * @self: a #FoundryKeyRotator
 * @host: the host name for the API key
 * @service_name: the service name for the API key
 * @secret: the current secret value
 *
 * Checks if the key rotator can rotate the API key for the given
 * host and service name.
 *
 * Returns: %TRUE if the key can be rotated, %FALSE otherwise
 *
 * Since: 1.1
 */
gboolean
foundry_key_rotator_can_rotate (FoundryKeyRotator *self,
                                const char        *host,
                                const char        *service_name,
                                const char        *secret)
{
  g_return_val_if_fail (FOUNDRY_IS_KEY_ROTATOR (self), FALSE);
  g_return_val_if_fail (host != NULL, FALSE);
  g_return_val_if_fail (service_name != NULL, FALSE);
  g_return_val_if_fail (secret != NULL, FALSE);

  return FOUNDRY_KEY_ROTATOR_GET_CLASS (self)->can_rotate (self, host, service_name, secret);
}

/**
 * foundry_key_rotator_rotate:
 * @self: a #FoundryKeyRotator
 * @host: the host name for the API key
 * @service_name: the service name for the API key
 * @secret: the current secret value
 * @expire_at: (nullable): when the new key should expire, or %NULL
 *
 * Rotates the API key for the given host and service name.
 *
 * Returns: (transfer full): a #DexFuture that resolves to a string
 *   containing the replacement secret, or rejects with an error
 *
 * Since: 1.1
 */
DexFuture *
foundry_key_rotator_rotate (FoundryKeyRotator *self,
                            const char        *host,
                            const char        *service_name,
                            const char        *secret,
                            GDateTime         *expire_at)
{
  dex_return_error_if_fail (FOUNDRY_IS_KEY_ROTATOR (self));
  dex_return_error_if_fail (host != NULL);
  dex_return_error_if_fail (service_name != NULL);
  dex_return_error_if_fail (secret != NULL);

  return FOUNDRY_KEY_ROTATOR_GET_CLASS (self)->rotate (self, host, service_name, secret, expire_at);
}

/**
 * foundry_key_rotator_check_expires_at:
 * @self: a [class@Foundry.KeyRotator]
 * @host: the hostname
 * @service_name: the name of the service such as "gitlab"
 * @secret: the current secret
 *
 * Checks when the key expires by querying the service.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a GDateTime or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_key_rotator_check_expires_at (FoundryKeyRotator *self,
                                      const char        *host,
                                      const char        *service_name,
                                      const char        *secret)
{
  dex_return_error_if_fail (FOUNDRY_IS_KEY_ROTATOR (self));
  dex_return_error_if_fail (host != NULL);
  dex_return_error_if_fail (service_name != NULL);
  dex_return_error_if_fail (secret != NULL);

  return FOUNDRY_KEY_ROTATOR_GET_CLASS (self)->check_expires_at (self, host, service_name, secret);
}
