/* foundry-lsp-client.c
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

#include <jsonrpc-glib.h>

#include "foundry-jsonrpc-private.h"
#include "foundry-lsp-client.h"
#include "foundry-lsp-provider.h"
#include "foundry-service-private.h"
#include "foundry-util.h"

struct _FoundryLspClient
{
  FoundryContextual   parent_instance;
  FoundryLspProvider *provider;
  JsonrpcClient      *client;
  GSubprocess        *subprocess;
  DexFuture          *future;
  GVariant           *capabilities;
};

struct _FoundryLspClientClass
{
  FoundryContextualClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryLspClient, foundry_lsp_client, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_CLIENT,
  PROP_IO_STREAM,
  PROP_PROVIDER,
  PROP_SUBPROCESS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_lsp_client_finalize (GObject *object)
{
  FoundryLspClient *self = (FoundryLspClient *)object;

  if (self->subprocess != NULL)
    g_subprocess_force_exit (self->subprocess);

  g_clear_object (&self->client);
  g_clear_object (&self->provider);
  g_clear_object (&self->subprocess);
  g_clear_pointer (&self->capabilities, g_variant_unref);
  dex_clear (&self->future);

  G_OBJECT_CLASS (foundry_lsp_client_parent_class)->finalize (object);
}

static void
foundry_lsp_client_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  FoundryLspClient *self = FOUNDRY_LSP_CLIENT (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, self->client);
      break;

    case PROP_PROVIDER:
      g_value_set_object (value, self->provider);
      break;

    case PROP_SUBPROCESS:
      g_value_set_object (value, self->subprocess);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_lsp_client_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  FoundryLspClient *self = FOUNDRY_LSP_CLIENT (object);

  switch (prop_id)
    {
    case PROP_IO_STREAM:
      self->client = jsonrpc_client_new (g_value_get_object (value));
      break;

    case PROP_PROVIDER:
      self->provider = g_value_dup_object (value);
      break;

    case PROP_SUBPROCESS:
      if ((self->subprocess = g_value_dup_object (value)))
        self->future = dex_subprocess_wait_check (self->subprocess);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_lsp_client_class_init (FoundryLspClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_lsp_client_finalize;
  object_class->get_property = foundry_lsp_client_get_property;
  object_class->set_property = foundry_lsp_client_set_property;

  properties[PROP_CLIENT] =
    g_param_spec_object ("client", NULL, NULL,
                         JSONRPC_TYPE_CLIENT,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_IO_STREAM] =
    g_param_spec_object ("io-stream", NULL, NULL,
                         G_TYPE_IO_STREAM,
                         (G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PROVIDER] =
    g_param_spec_object ("provider", NULL, NULL,
                         FOUNDRY_TYPE_LSP_PROVIDER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBPROCESS] =
    g_param_spec_object ("subprocess", NULL, NULL,
                         G_TYPE_SUBPROCESS,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_lsp_client_init (FoundryLspClient *self)
{
}

/**
 * foundry_lsp_client_query_capabilities:
 * @self: a #FoundryLspClient
 *
 * Queries the servers capabilities.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when
 *   the query completes or fails.
 */
DexFuture *
foundry_lsp_client_query_capabilities (FoundryLspClient *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (self));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "not supported");
}

/**
 * foundry_lsp_client_call:
 * @self: a #FoundryLspClient
 * @method: the method name to call
 * @params: parameters for the method call
 *
 * If @params is floating, the reference will be consumed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when
 *   a reply is received for the method call.
 */
DexFuture *
foundry_lsp_client_call (FoundryLspClient *self,
                         const char       *method,
                         GVariant         *params)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (self));

  return _jsonrpc_client_call (self->client, method, params);
}

/**
 * foundry_lsp_client_notify:
 * @self: a #FoundryLspClient
 * @method: the method name to call
 * @params: parameters for the notification
 *
 * If @params is floating, the reference will be consumed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when
 *   the notification has been sent.
 */
DexFuture *
foundry_lsp_client_notify (FoundryLspClient *self,
                           const char       *method,
                           GVariant         *params)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (self));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "not supported");
}

static DexFuture *
foundry_lsp_client_load_fiber (gpointer data)
{
  FoundryLspClient *self = data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GVariant) initialize_params = NULL;
  g_autoptr(GVariant) initialization_options = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_dir = NULL;
  g_autofree char *root_uri = NULL;
  g_autofree char *root_path = NULL;
  g_autofree char *basename = NULL;
  const char *trace_string = "off";

  g_assert (FOUNDRY_IS_LSP_CLIENT (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  project_dir = foundry_context_dup_project_directory (context);
  root_uri = g_file_get_uri (project_dir);
  basename = g_file_get_basename (project_dir);

  if (g_file_is_native (project_dir))
    root_path = g_file_get_path (project_dir);
  else
    root_path = g_strdup ("");

  if (self->provider != NULL)
    initialization_options = foundry_lsp_provider_dup_initialization_options (self->provider);

  initialize_params = JSONRPC_MESSAGE_NEW (
#if 0
    /* Some LSPs will monitor the PID of the editor and exit when they
     * detect the editor has exited. Since we are likely in a different
     * PID namespace than the LSP, there is a PID mismatch and it will
     * probably get PID 2 (from Flatpak) and not be of any use.
     *
     * Just ignore it as the easiest solution.
     *
     * If this causes problems elsewhere, we might need to try to setup
     * a quirk handler for some LSPs.
     *
     * https://gitlab.gnome.org/GNOME/gnome-builder/-/issues/2050
     */
    "processId", JSONRPC_MESSAGE_PUT_INT64 (getpid ()),
#endif
    "rootUri", JSONRPC_MESSAGE_PUT_STRING (root_uri),
    "clientInfo", "{",
      "name", JSONRPC_MESSAGE_PUT_STRING ("Foundry"),
      "version", JSONRPC_MESSAGE_PUT_STRING (PACKAGE_VERSION),
    "}",
    "rootPath", JSONRPC_MESSAGE_PUT_STRING (root_path),
    "workspaceFolders", "[",
      "{",
        "uri", JSONRPC_MESSAGE_PUT_STRING (root_uri),
        "name", JSONRPC_MESSAGE_PUT_STRING (basename),
      "}",
    "]",
    "trace", JSONRPC_MESSAGE_PUT_STRING (trace_string),
    "capabilities", "{",
      "workspace", "{",
        "applyEdit", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
        "configuration", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
        "symbol", "{",
          "SymbolKind", "{",
            "valueSet", "[",
              JSONRPC_MESSAGE_PUT_INT64 (1), /* File */
              JSONRPC_MESSAGE_PUT_INT64 (2), /* Module */
              JSONRPC_MESSAGE_PUT_INT64 (3), /* Namespace */
              JSONRPC_MESSAGE_PUT_INT64 (4), /* Package */
              JSONRPC_MESSAGE_PUT_INT64 (5), /* Class */
              JSONRPC_MESSAGE_PUT_INT64 (6), /* Method */
              JSONRPC_MESSAGE_PUT_INT64 (7), /* Property */
              JSONRPC_MESSAGE_PUT_INT64 (8), /* Field */
              JSONRPC_MESSAGE_PUT_INT64 (9), /* Constructor */
              JSONRPC_MESSAGE_PUT_INT64 (10), /* Enum */
              JSONRPC_MESSAGE_PUT_INT64 (11), /* Interface */
              JSONRPC_MESSAGE_PUT_INT64 (12), /* Function */
              JSONRPC_MESSAGE_PUT_INT64 (13), /* Variable */
              JSONRPC_MESSAGE_PUT_INT64 (14), /* Constant */
              JSONRPC_MESSAGE_PUT_INT64 (15), /* String */
              JSONRPC_MESSAGE_PUT_INT64 (16), /* Number */
              JSONRPC_MESSAGE_PUT_INT64 (17), /* Boolean */
              JSONRPC_MESSAGE_PUT_INT64 (18), /* Array */
              JSONRPC_MESSAGE_PUT_INT64 (19), /* Object */
              JSONRPC_MESSAGE_PUT_INT64 (20), /* Key */
              JSONRPC_MESSAGE_PUT_INT64 (21), /* Null */
              JSONRPC_MESSAGE_PUT_INT64 (22), /* EnumMember */
              JSONRPC_MESSAGE_PUT_INT64 (23), /* Struct */
              JSONRPC_MESSAGE_PUT_INT64 (24), /* Event */
              JSONRPC_MESSAGE_PUT_INT64 (25), /* Operator */
              JSONRPC_MESSAGE_PUT_INT64 (26), /* TypeParameter */
            "]",
          "}",
        "}",
      "}",
      "textDocument", "{",
        "completion", "{",
          "contextSupport", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
          "completionItem", "{",
            "snippetSupport", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
            "documentationFormat", "[",
              "markdown",
              "plaintext",
            "]",
            "deprecatedSupport", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
          "}",
          "completionItemKind", "{",
            "valueSet", "[",
              JSONRPC_MESSAGE_PUT_INT64 (1),
              JSONRPC_MESSAGE_PUT_INT64 (2),
              JSONRPC_MESSAGE_PUT_INT64 (3),
              JSONRPC_MESSAGE_PUT_INT64 (4),
              JSONRPC_MESSAGE_PUT_INT64 (5),
              JSONRPC_MESSAGE_PUT_INT64 (6),
              JSONRPC_MESSAGE_PUT_INT64 (7),
              JSONRPC_MESSAGE_PUT_INT64 (8),
              JSONRPC_MESSAGE_PUT_INT64 (9),
              JSONRPC_MESSAGE_PUT_INT64 (10),
              JSONRPC_MESSAGE_PUT_INT64 (11),
              JSONRPC_MESSAGE_PUT_INT64 (12),
              JSONRPC_MESSAGE_PUT_INT64 (13),
              JSONRPC_MESSAGE_PUT_INT64 (14),
              JSONRPC_MESSAGE_PUT_INT64 (15),
              JSONRPC_MESSAGE_PUT_INT64 (16),
              JSONRPC_MESSAGE_PUT_INT64 (17),
              JSONRPC_MESSAGE_PUT_INT64 (18),
              JSONRPC_MESSAGE_PUT_INT64 (19),
              JSONRPC_MESSAGE_PUT_INT64 (20),
              JSONRPC_MESSAGE_PUT_INT64 (21),
              JSONRPC_MESSAGE_PUT_INT64 (22),
              JSONRPC_MESSAGE_PUT_INT64 (23),
              JSONRPC_MESSAGE_PUT_INT64 (24),
              JSONRPC_MESSAGE_PUT_INT64 (25),
            "]",
          "}",
        "}",
        "diagnostic", "{",
        "}",
        "hover", "{",
          "contentFormat", "[",
            "markdown",
            "plaintext",
          "]",
        "}",
        "publishDiagnostics", "{",
          "tagSupport", "{",
            "valueSet", "[",
              JSONRPC_MESSAGE_PUT_INT64 (1),
              JSONRPC_MESSAGE_PUT_INT64 (2),
            "]",
          "}",
        "}",
        "codeAction", "{",
          "dynamicRegistration", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
          "isPreferredSupport", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
          "codeActionLiteralSupport", "{",
            "codeActionKind", "{",
              "valueSet", "[",
                "",
                "quickfix",
                "refactor",
                "refactor.extract",
                "refactor.inline",
                "refactor.rewrite",
                "source",
                "source.organizeImports",
              "]",
            "}",
          "}",
        "}",
      "}",
      "window", "{",
        "workDoneProgress", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
      "}",
    "}",
    "initializationOptions", "{",
      JSONRPC_MESSAGE_PUT_VARIANT (initialization_options),
    "}"
  );

  if (!(reply = dex_await_variant (_jsonrpc_client_call (self->client, "initialize", initialize_params), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  JSONRPC_MESSAGE_PARSE (reply, "capabilities", JSONRPC_MESSAGE_GET_VARIANT (&self->capabilities));

  return dex_future_new_true ();
}

DexFuture *
foundry_lsp_client_new (FoundryContext *context,
                        GIOStream      *io_stream,
                        GSubprocess    *subprocess)
{
  g_autoptr(FoundryLspClient) client = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));
  dex_return_error_if_fail (G_IS_IO_STREAM (io_stream));
  dex_return_error_if_fail (!subprocess || G_IS_SUBPROCESS (subprocess));

  client = g_object_new (FOUNDRY_TYPE_LSP_CLIENT,
                         "context", context,
                         "io-stream", io_stream,
                         "subprocess", subprocess,
                         NULL);

  return dex_future_then (dex_scheduler_spawn (NULL, 0,
                                               foundry_lsp_client_load_fiber,
                                               g_object_ref (client),
                                               g_object_unref),
                          foundry_future_return_object,
                          g_object_ref (client),
                          g_object_unref);
}

/**
 * foundry_lsp_client_await:
 * @self: a [class@Foundry.LspClient]
 *
 * Await completion of the client subprocess.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves
 *   when the subprocess has exited or rejects with error.
 */
DexFuture *
foundry_lsp_client_await (FoundryLspClient *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (self));

  if (self->future == NULL)
    return dex_future_new_true ();

  return dex_ref (self->future);
}

gboolean
foundry_lsp_client_supports_language (FoundryLspClient *self,
                                      const char       *language_id)
{
  g_autoptr(PeasPluginInfo) plugin_info = NULL;

  g_return_val_if_fail (FOUNDRY_IS_LSP_CLIENT (self), FALSE);
  g_return_val_if_fail (language_id != NULL, FALSE);

  if ((plugin_info = foundry_lsp_provider_dup_plugin_info (self->provider)))
    {
      const char *x_languages = peas_plugin_info_get_external_data (plugin_info, "LSP-Languages");
      g_auto(GStrv) languages = g_strsplit (x_languages, ";", 0);

      for (guint i = 0; languages[i]; i++)
        {
          if (g_strcmp0 (languages[i], language_id) == 0)
            return TRUE;
        }
    }

  return FALSE;
}
