/* foundry-secret-service.c
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

#include <libdex.h>
#include <libsecret/secret.h>

#include "foundry-secret-private.h"
#include "foundry-secret-service.h"
#include "foundry-service-private.h"

/**
 * FoundrySecretService:
 *
 * Manages secure storage and retrieval of API keys and sensitive data.
 *
 * FoundrySecretService provides a secure interface for storing and retrieving
 * API keys, authentication tokens, and other sensitive information. It integrates
 * with the system's secret storage backend and provides a unified API for
 * managing credentials across different services and platforms.
 */

typedef struct {
  char *host;
  char *service;
} FoundrySecretHostService;

#define FOUNDRY_SECRET_SCHEMA_NAME       "app.devsuite.foundry.secret.api-key"
#define FOUNDRY_SECRET_ATTRIBUTE_HOST    "host"
#define FOUNDRY_SECRET_ATTRIBUTE_SERVICE "service"

static SecretSchema foundry_api_key_schema = {
  .name = FOUNDRY_SECRET_SCHEMA_NAME,
  .flags = SECRET_SCHEMA_DONT_MATCH_NAME,
  .attributes = {
    { FOUNDRY_SECRET_ATTRIBUTE_HOST, SECRET_SCHEMA_ATTRIBUTE_STRING },
    { FOUNDRY_SECRET_ATTRIBUTE_SERVICE, SECRET_SCHEMA_ATTRIBUTE_STRING },
    { NULL, 0 }
  }
};

struct _FoundrySecretService
{
  FoundryService parent_instance;
};

struct _FoundrySecretServiceClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundrySecretService, foundry_secret_service, FOUNDRY_TYPE_SERVICE)

static void
foundry_secret_service_class_init (FoundrySecretServiceClass *klass)
{
}

static void
foundry_secret_service_init (FoundrySecretService *self)
{
}

static GHashTable *
create_attributes (const char *host,
                   const char *service)
{
  GHashTable *attributes;

  attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_hash_table_replace (attributes,
                        g_strdup (FOUNDRY_SECRET_ATTRIBUTE_HOST),
                        g_strdup (host));
  g_hash_table_replace (attributes,
                        g_strdup (FOUNDRY_SECRET_ATTRIBUTE_SERVICE),
                        g_strdup (service));

  return attributes;
}

/**
 * foundry_secret_service_store_api_key:
 * @self: a [class@Foundry.SecretService]
 * @host: the hostname of the api server
 * @service: the name of the service such as "gitlab"
 * @api_key: the actual API key
 *
 * Stores an API key in secret storage.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value on success or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_secret_service_store_api_key (FoundrySecretService *self,
                                      const char           *host,
                                      const char           *service,
                                      const char           *api_key)
{
  g_return_val_if_fail (FOUNDRY_IS_SECRET_SERVICE (self), NULL);
  g_return_val_if_fail (host != NULL, NULL);
  g_return_val_if_fail (service != NULL, NULL);
  g_return_val_if_fail (api_key != NULL, NULL);

  return foundry_secret_password_storev (&foundry_api_key_schema,
                                         create_attributes (host, service),
                                         SECRET_COLLECTION_DEFAULT,
                                         "Foundry API Key",
                                         api_key);
}

/**
 * foundry_secret_service_lookup_api_key:
 * @self: a [class@Foundry.SecretService]
 * @host: the hostname of the api server
 * @service: the name of the service such as "gitlab"
 *
 * Retrieves an API key from secret storage.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a string on success or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_secret_service_lookup_api_key (FoundrySecretService *self,
                                       const char           *host,
                                       const char           *service)
{
  g_return_val_if_fail (FOUNDRY_IS_SECRET_SERVICE (self), NULL);
  g_return_val_if_fail (host != NULL, NULL);
  g_return_val_if_fail (service != NULL, NULL);

  return foundry_secret_password_lookupv (&foundry_api_key_schema,
                                          create_attributes (host, service));
}

/**
 * foundry_secret_service_delete_api_key:
 * @self: a [class@Foundry.SecretService]
 * @host: the hostname of the api server
 * @service: the name of the service such as "gitlab"
 *
 * Removes an API key from secret storage.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   any value on success or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_secret_service_delete_api_key (FoundrySecretService *self,
                                       const char           *host,
                                       const char           *service)
{
  g_return_val_if_fail (FOUNDRY_IS_SECRET_SERVICE (self), NULL);
  g_return_val_if_fail (host != NULL, NULL);
  g_return_val_if_fail (service != NULL, NULL);

  return foundry_secret_password_clearv (&foundry_api_key_schema,
                                         create_attributes (host, service));
}
