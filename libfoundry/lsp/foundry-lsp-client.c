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

#include "foundry-diagnostic.h"
#include "foundry-diagnostic-builder.h"
#include "foundry-json-node.h"
#include "foundry-jsonrpc-driver-private.h"
#include "foundry-lsp-client-private.h"
#include "foundry-lsp-provider.h"
#include "foundry-operation-manager.h"
#include "foundry-operation.h"
#include "foundry-text-buffer.h"
#include "foundry-text-document.h"
#include "foundry-text-iter.h"
#include "foundry-text-manager.h"
#include "foundry-util-private.h"

struct _FoundryLspClient
{
  FoundryContextual     parent_instance;
  FoundryLspProvider   *provider;
  FoundryJsonrpcDriver *driver;
  GSubprocess          *subprocess;
  DexFuture            *future;
  JsonNode             *capabilities;
  GHashTable           *diagnostics;
  GHashTable           *progress;
  GHashTable           *commit_notify;
  guint                 text_document_sync : 2;
};

struct _FoundryLspClientClass
{
  FoundryContextualClass parent_class;
};

typedef struct _CommitNotify
{
  FoundryLspClient *self;
  GWeakRef buffer_wr;
  GWeakRef document_wr;
  guint handler_id;
} CommitNotify;

G_DEFINE_FINAL_TYPE (FoundryLspClient, foundry_lsp_client, FOUNDRY_TYPE_CONTEXTUAL)

enum {
  PROP_0,
  PROP_IO_STREAM,
  PROP_PROVIDER,
  PROP_SUBPROCESS,
  N_PROPS
};

enum {
  TEXT_DOCUMENT_SYNC_NONE,
  TEXT_DOCUMENT_SYNC_FULL,
  TEXT_DOCUMENT_SYNC_INCREMENTAL,
};

static GParamSpec *properties[N_PROPS];

static void
commit_notify_free (CommitNotify *notify)
{
  if (notify->handler_id != 0)
    {
      g_autoptr(FoundryTextBuffer) buffer = g_weak_ref_get (&notify->buffer_wr);

      if (buffer != NULL)
        foundry_text_buffer_remove_commit_notify (buffer, notify->handler_id);
    }

  g_weak_ref_clear (&notify->buffer_wr);
  g_weak_ref_clear (&notify->document_wr);
  g_free (notify);
}

static char *
translate_language_id (char *language_id)
{
  g_autofree char *freeme = language_id;

  if (g_str_equal (language_id, "python3"))
    return g_strdup ("python");

  return g_steal_pointer (&freeme);
}

static gboolean
foundry_lsp_client_window_work_done_progress_create (FoundryLspClient *self,
                                                     JsonNode         *params,
                                                     JsonNode         *id)
{
  g_autoptr(FoundryOperationManager) operation_manager = NULL;
  g_autoptr(FoundryOperation) operation = NULL;
  g_autoptr(FoundryContext) context = NULL;
  JsonNode *token = NULL;

  g_assert (FOUNDRY_IS_LSP_CLIENT (self));
  g_assert (id != NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (params, "token", FOUNDRY_JSON_NODE_GET_NODE (&token)))
    return FALSE;

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    return FALSE;

  if (!(operation_manager = foundry_context_dup_operation_manager (context)))
    return FALSE;

  operation = foundry_operation_manager_begin (operation_manager, "");

  g_hash_table_replace (self->progress,
                        json_node_copy (token),
                        g_steal_pointer (&operation));

  dex_future_disown (foundry_jsonrpc_driver_reply (self->driver, id, NULL));

  return TRUE;
}

static gboolean
foundry_lsp_client_progress (FoundryLspClient *self,
                             JsonNode         *params,
                             JsonNode         *id)
{
  FoundryOperation *operation = NULL;
  JsonNode *token = NULL;
  JsonNode *value = NULL;
  const char *kind = NULL;

  g_assert (FOUNDRY_IS_LSP_CLIENT (self));
  g_assert (params != NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (params,
                                  "token", FOUNDRY_JSON_NODE_GET_NODE (&token),
                                  "value", FOUNDRY_JSON_NODE_GET_NODE (&value),
                                  "kind", FOUNDRY_JSON_NODE_GET_STRING (&kind)) ||
      kind == NULL)
    return FALSE;

  if (!(operation = g_hash_table_lookup (self->progress, token)))
    return FALSE;

  if (g_str_equal (kind, "begin"))
    {
      const char *title = NULL;
      const char *message = NULL;
      gint64 percentage = 0;

      if (FOUNDRY_JSON_OBJECT_PARSE (value, "title", FOUNDRY_JSON_NODE_GET_STRING (&title)))
        foundry_operation_set_title (operation, title);

      if (FOUNDRY_JSON_OBJECT_PARSE (value, "message", FOUNDRY_JSON_NODE_GET_STRING (&message)))
        foundry_operation_set_subtitle (operation, message);

      if (FOUNDRY_JSON_OBJECT_PARSE (value, "percentage", FOUNDRY_JSON_NODE_GET_INT (&percentage)))
        foundry_operation_set_progress (operation, CLAMP ((double)percentage / 100.0, 0.0, 1.0));
    }
  else if (g_str_equal (kind, "report"))
    {
      const char *message = NULL;
      gint64 percentage = 0;

      if (FOUNDRY_JSON_OBJECT_PARSE (value, "message", FOUNDRY_JSON_NODE_GET_STRING (&message)))
        foundry_operation_set_subtitle (operation, message);

      if (FOUNDRY_JSON_OBJECT_PARSE (value, "percentage", FOUNDRY_JSON_NODE_GET_INT (&percentage)))
        foundry_operation_set_progress (operation, CLAMP ((double)percentage / 100.0, 0.0, 1.0));
    }
  else if (g_str_equal (kind, "end"))
    {
      const char *message = NULL;

      if (FOUNDRY_JSON_OBJECT_PARSE (value, "message", FOUNDRY_JSON_NODE_GET_STRING (&message)))
        foundry_operation_set_subtitle (operation, message);

      foundry_operation_complete (operation);
      g_hash_table_remove (self->progress, token);
    }

  return TRUE;
}

static FoundryDiagnosticSeverity
map_lsp_severity (gint64 lsp_severity)
{
  switch (lsp_severity)
    {
    case 1: /* Error */
      return FOUNDRY_DIAGNOSTIC_ERROR;
    case 2: /* Warning */
      return FOUNDRY_DIAGNOSTIC_WARNING;
    case 3: /* Information */
    case 4: /* Hint */
      return FOUNDRY_DIAGNOSTIC_NOTE;
    default:
      return FOUNDRY_DIAGNOSTIC_ERROR;
    }
}

static void
foundry_lsp_client_publish_diagnostics (FoundryLspClient *self,
                                        JsonNode         *params)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(FoundryContext) context = NULL;
  const char *uri = NULL;
  JsonNode *diagnostics = NULL;
  GListStore *store;
  JsonArray *diagnostics_array = NULL;

  g_assert (FOUNDRY_IS_LSP_CLIENT (self));
  g_assert (params != NULL);

  if (!FOUNDRY_JSON_OBJECT_PARSE (params,
                                  "uri", FOUNDRY_JSON_NODE_GET_STRING (&uri),
                                  "diagnostics", FOUNDRY_JSON_NODE_GET_NODE (&diagnostics)))
    return;

  file = g_file_new_for_uri (uri);

  if (!(store = g_hash_table_lookup (self->diagnostics, file)))
    return;

  g_list_store_remove_all (store);

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self))))
    return;

  if (!diagnostics || !JSON_NODE_HOLDS_ARRAY (diagnostics))
    return;

  diagnostics_array = json_node_get_array (diagnostics);

  {
    guint n_diagnostics = json_array_get_length (diagnostics_array);

    for (guint i = 0; i < n_diagnostics; i++)
      {
        JsonNode *diagnostic_node = json_array_get_element (diagnostics_array, i);
        g_autoptr(FoundryDiagnosticBuilder) builder = NULL;
        g_autoptr(FoundryDiagnostic) diagnostic = NULL;
        g_autofree char *code_str = NULL;
        const char *message = NULL;
        const char *code = NULL;
        gint64 severity = 0;
        gint64 start_line = 0;
        gint64 start_character = 0;
        gint64 end_line = 0;
        gint64 end_character = 0;
        gint64 code_int = -1;

        if (!FOUNDRY_JSON_OBJECT_PARSE (diagnostic_node,
                                        "message", FOUNDRY_JSON_NODE_GET_STRING (&message),
                                        "range", "{",
                                          "start", "{",
                                            "line", FOUNDRY_JSON_NODE_GET_INT (&start_line),
                                            "character", FOUNDRY_JSON_NODE_GET_INT (&start_character),
                                          "}",
                                          "end", "{",
                                            "line", FOUNDRY_JSON_NODE_GET_INT (&end_line),
                                            "character", FOUNDRY_JSON_NODE_GET_INT (&end_character),
                                          "}",
                                        "}"))
          continue;

        if (!FOUNDRY_JSON_OBJECT_PARSE (diagnostic_node,
                                        "severity", FOUNDRY_JSON_NODE_GET_INT (&severity)))
          severity = 0;

        if (!FOUNDRY_JSON_OBJECT_PARSE (diagnostic_node, "code", FOUNDRY_JSON_NODE_GET_STRING (&code)) &&
            FOUNDRY_JSON_OBJECT_PARSE (diagnostic_node, "code", FOUNDRY_JSON_NODE_GET_INT (&code_int)))
          code = code_str = g_strdup_printf ("%"G_GINT64_FORMAT, code_int);

        builder = foundry_diagnostic_builder_new (context);
        foundry_diagnostic_builder_set_file (builder, file);
        foundry_diagnostic_builder_set_message (builder, message);
        foundry_diagnostic_builder_set_severity (builder, map_lsp_severity (severity));

        if (code != NULL)
          foundry_diagnostic_builder_set_rule_id (builder, code);

        foundry_diagnostic_builder_set_line (builder, start_line);
        foundry_diagnostic_builder_set_line_offset (builder, start_character);
        foundry_diagnostic_builder_add_range (builder,
                                              start_line, start_character,
                                              end_line, end_character);

        diagnostic = foundry_diagnostic_builder_end (builder);

        if (diagnostic != NULL)
          g_list_store_append (store, diagnostic);
      }
  }
}

static gboolean
foundry_lsp_client_handle_method_call (FoundryLspClient *self,
                                       const char       *method,
                                       JsonNode         *params,
                                       JsonNode         *id)
{
  g_assert (FOUNDRY_IS_LSP_CLIENT (self));
  g_assert (method != NULL);
  g_assert (id != NULL);

  g_debug ("Received method call `%s`", method);

  if (params && strcmp (method, "window/workDoneProgress/create") == 0)
    return foundry_lsp_client_window_work_done_progress_create (self, params, id);
  else if (params && strcmp (method, "$/progress") == 0)
    return foundry_lsp_client_progress (self, params, id);

  return FALSE;
}

static void
foundry_lsp_client_handle_notification (FoundryLspClient *self,
                                        const char       *method,
                                        JsonNode         *params)
{
  g_assert (FOUNDRY_IS_LSP_CLIENT (self));
  g_assert (method != NULL);

  if (params && strcmp (method, "textDocument/publishDiagnostics") == 0)
    foundry_lsp_client_publish_diagnostics (self, params);
}

static void
foundry_lsp_client_set_io_stream (FoundryLspClient *self,
                                  GIOStream        *io_stream)
{
  g_assert (FOUNDRY_IS_LSP_CLIENT (self));
  g_assert (G_IS_IO_STREAM (io_stream));

  self->driver = foundry_jsonrpc_driver_new (io_stream, FOUNDRY_JSONRPC_STYLE_HTTP);

  g_signal_connect_object (self->driver,
                           "handle-method-call",
                           G_CALLBACK (foundry_lsp_client_handle_method_call),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->driver,
                           "handle-notification",
                           G_CALLBACK (foundry_lsp_client_handle_notification),
                           self,
                           G_CONNECT_SWAPPED);
}

static DexFuture *
foundry_lsp_client_subprocess_exited_cb (DexFuture *completed,
                                         gpointer   user_data)
{
  FoundryWeakPair *pair = user_data;
  g_autoptr(FoundryLspClient) client = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;

  g_assert (pair != NULL);

  if (foundry_weak_pair_get (pair, &client, &subprocess))
    {
      const char *identifier = g_subprocess_get_identifier (subprocess);

      FOUNDRY_CONTEXTUAL_MESSAGE (client, "Language server %s exited", identifier);
    }

  return NULL;
}

static void
foundry_lsp_client_constructed (GObject *object)
{
  FoundryLspClient *self = (FoundryLspClient *)object;

  G_OBJECT_CLASS (foundry_lsp_client_parent_class)->constructed (object);

  if (self->driver)
    foundry_jsonrpc_driver_start (self->driver);
}

static void
foundry_lsp_client_finalize (GObject *object)
{
  FoundryLspClient *self = (FoundryLspClient *)object;

  if (self->driver != NULL)
    foundry_jsonrpc_driver_stop (self->driver);

  if (self->subprocess != NULL)
    g_subprocess_force_exit (self->subprocess);

  g_clear_object (&self->driver);
  g_clear_object (&self->provider);
  g_clear_object (&self->subprocess);
  g_clear_pointer (&self->capabilities, json_node_unref);
  g_clear_pointer (&self->diagnostics, g_hash_table_unref);
  g_clear_pointer (&self->progress, g_hash_table_unref);
  g_clear_pointer (&self->commit_notify, g_hash_table_unref);
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
      foundry_lsp_client_set_io_stream (self, g_value_get_object (value));
      break;

    case PROP_PROVIDER:
      self->provider = g_value_dup_object (value);
      break;

    case PROP_SUBPROCESS:
      if ((self->subprocess = g_value_dup_object (value)))
        {
          self->future = dex_subprocess_wait_check (self->subprocess);

          /* This helps us get a log message when the process has exited, but also
           * ensures that our future is kept alive whether or not someone calling
           * foundry_lsp_client_await() has discarded their future. Otherwise we
           * could end up killing the process on every requested LSP operation
           * being completed.
           */
          dex_future_disown (dex_future_finally (dex_ref (self->future),
                                                 foundry_lsp_client_subprocess_exited_cb,
                                                 foundry_weak_pair_new (self, self->subprocess),
                                                 (GDestroyNotify) foundry_weak_pair_free));
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_lsp_client_class_init (FoundryLspClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = foundry_lsp_client_constructed;
  object_class->finalize = foundry_lsp_client_finalize;
  object_class->get_property = foundry_lsp_client_get_property;
  object_class->set_property = foundry_lsp_client_set_property;

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
  self->diagnostics = g_hash_table_new_full ((GHashFunc) g_file_hash,
                                             (GEqualFunc) g_file_equal,
                                             g_object_unref,
                                             g_object_unref);
  self->progress = g_hash_table_new_full ((GHashFunc) json_node_hash,
                                          (GEqualFunc) json_node_equal,
                                          (GDestroyNotify) json_node_unref,
                                          g_object_unref);
  self->commit_notify = g_hash_table_new_full ((GHashFunc) g_file_hash,
                                               (GEqualFunc) g_file_equal,
                                               g_object_unref,
                                               (GDestroyNotify) commit_notify_free);
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

  if (self->capabilities != NULL)
    return dex_future_new_take_boxed (JSON_TYPE_NODE, json_node_ref (self->capabilities));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "not supported");
}

static DexFuture *
foundry_lsp_client_call_cb (DexFuture *completed,
                            gpointer   user_data)
{
  const GValue *value;

  if ((value = dex_future_get_value (completed, NULL)) &&
      G_VALUE_HOLDS (value, JSON_TYPE_NODE))
    return dex_ref (completed);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_CANCELLED,
                                "Subprocess exited during JSONRPC call");
}

/**
 * foundry_lsp_client_call:
 * @self: a #FoundryLspClient
 * @method: the method name to call
 * @params: (nullable): parameters for the method call
 *
 * If @params is floating, the reference will be consumed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when
 *   a reply is received for the method call.
 */
DexFuture *
foundry_lsp_client_call (FoundryLspClient *self,
                         const char       *method,
                         JsonNode         *params)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (self));
  dex_return_error_if_fail (method != NULL);

  /* We want to return the call from the driver, but if our subprocess
   * LSP exits we want to early bail from the whole thing.
   */
  return dex_future_finally (dex_future_first (dex_ref (self->future),
                                               foundry_jsonrpc_driver_call (self->driver, method, params),
                                               NULL),
                             foundry_lsp_client_call_cb,
                             NULL, NULL);
}

/**
 * foundry_lsp_client_notify:
 * @self: a #FoundryLspClient
 * @method: the method name to call
 * @params: (nullable): parameters for the notification
 *
 * If @params is floating, the reference will be consumed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when
 *   the notification has been sent.
 */
DexFuture *
foundry_lsp_client_notify (FoundryLspClient *self,
                           const char       *method,
                           JsonNode         *params)
{
  dex_return_error_if_fail (FOUNDRY_IS_LSP_CLIENT (self));
  dex_return_error_if_fail (method != NULL);

  return foundry_jsonrpc_driver_notify (self->driver, method, params);
}

static void
foundry_lsp_client_buffer_commit_notify (FoundryTextBuffer            *buffer,
                                         FoundryTextBufferNotifyFlags  flags,
                                         guint                         position,
                                         guint                         length,
                                         gpointer                      user_data)
{
  CommitNotify *notify = user_data;
  g_autoptr(FoundryTextDocument) document = NULL;
  g_autoptr(GFile) file = NULL;
  FoundryLspClient *self;

  g_assert (FOUNDRY_IS_TEXT_BUFFER (buffer));
  g_assert (notify != NULL);

  if (!(document = g_weak_ref_get (&notify->document_wr)) ||
      !(file = foundry_text_document_dup_file (document)) ||
      !(self = notify->self))
    return;

  if (flags == FOUNDRY_TEXT_BUFFER_NOTIFY_BEFORE_INSERT)
    {
      g_assert_not_reached ();
    }
  else if (flags == FOUNDRY_TEXT_BUFFER_NOTIFY_AFTER_INSERT)
    {
      if (self->text_document_sync == TEXT_DOCUMENT_SYNC_INCREMENTAL)
        {
          g_autoptr(JsonNode) params = NULL;
          g_autofree char *uri = NULL;
          g_autofree char *copy = NULL;
          FoundryTextIter begin;
          FoundryTextIter end;
          gint64 version;
          guint line;
          guint column;

          foundry_text_buffer_get_iter_at_offset (buffer, &begin, position);
          foundry_text_buffer_get_iter_at_offset (buffer, &end, position + length);

          uri = foundry_text_document_dup_uri (document);
          copy = foundry_text_iter_get_slice (&begin, &end);

          version = (gint64)foundry_text_buffer_get_change_count (buffer) + 1;

          line = foundry_text_iter_get_line (&begin);
          column = foundry_text_iter_get_line_offset (&begin);

          params = FOUNDRY_JSON_OBJECT_NEW (
            "textDocument", "{",
              "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
              "version", FOUNDRY_JSON_NODE_PUT_INT (version),
            "}",
            "contentChanges", "[",
              "{",
                "range", "{",
                  "start", "{",
                    "line", FOUNDRY_JSON_NODE_PUT_INT (line),
                    "character", FOUNDRY_JSON_NODE_PUT_INT (column),
                  "}",
                  "end", "{",
                    "line", FOUNDRY_JSON_NODE_PUT_INT (line),
                    "character", FOUNDRY_JSON_NODE_PUT_INT (column),
                  "}",
                "}",
                "rangeLength", FOUNDRY_JSON_NODE_PUT_INT (0),
                "text", FOUNDRY_JSON_NODE_PUT_STRING (copy),
              "}",
            "]");

          dex_future_disown (foundry_lsp_client_notify (self, "textDocument/didChange", params));
        }
      else if (self->text_document_sync == TEXT_DOCUMENT_SYNC_FULL)
        {
          g_autoptr(GBytes) content = NULL;
          g_autoptr(JsonNode) params = NULL;
          g_autofree char *uri = NULL;
          const char *text;
          gint64 version;

          uri = foundry_text_document_dup_uri (document);
          version = (gint64)foundry_text_buffer_get_change_count (buffer);

          content = foundry_text_buffer_dup_contents (buffer);
          text = (const char *)g_bytes_get_data (content, NULL);

          params = FOUNDRY_JSON_OBJECT_NEW (
            "textDocument", "{",
              "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
              "version", FOUNDRY_JSON_NODE_PUT_INT (version),
            "}",
            "contentChanges", "[",
              "{",
                "text", FOUNDRY_JSON_NODE_PUT_STRING (text),
              "}",
            "]");

          dex_future_disown (foundry_lsp_client_notify (self, "textDocument/didChange", params));
        }
    }
  else if (flags == FOUNDRY_TEXT_BUFFER_NOTIFY_BEFORE_DELETE)
    {
      if (self->text_document_sync == TEXT_DOCUMENT_SYNC_INCREMENTAL)
        {
          g_autoptr(JsonNode) params = NULL;
          g_autofree char *uri = NULL;
          FoundryTextIter copy_begin;
          FoundryTextIter copy_end;
          struct {
            int line;
            int column;
          } begin, end;
          gint64 version;

          uri = foundry_text_document_dup_uri (document);

          /* We get called before this change is registered */
          version = (int)foundry_text_buffer_get_change_count (buffer) + 1;

          foundry_text_buffer_get_iter_at_offset (buffer, &copy_begin, position);
          foundry_text_buffer_get_iter_at_offset (buffer, &copy_end, position + length);

          begin.line = foundry_text_iter_get_line (&copy_begin);
          begin.column = foundry_text_iter_get_line_offset (&copy_begin);

          end.line = foundry_text_iter_get_line (&copy_end);
          end.column = foundry_text_iter_get_line_offset (&copy_end);

          params = FOUNDRY_JSON_OBJECT_NEW (
            "textDocument", "{",
              "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
              "version", FOUNDRY_JSON_NODE_PUT_INT (version),
            "}",
            "contentChanges", "[",
              "{",
                "range", "{",
                  "start", "{",
                    "line", FOUNDRY_JSON_NODE_PUT_INT (begin.line),
                    "character", FOUNDRY_JSON_NODE_PUT_INT (begin.column),
                  "}",
                  "end", "{",
                    "line", FOUNDRY_JSON_NODE_PUT_INT (end.line),
                    "character", FOUNDRY_JSON_NODE_PUT_INT (end.column),
                  "}",
                "}",
                "rangeLength", FOUNDRY_JSON_NODE_PUT_INT (length),
                "text", FOUNDRY_JSON_NODE_PUT_STRING (""),
              "}",
            "]");

          dex_future_disown (foundry_lsp_client_notify (self, "textDocument/didChange", params));
        }
    }
  else if (flags == FOUNDRY_TEXT_BUFFER_NOTIFY_AFTER_DELETE)
    {
      if (self->text_document_sync == TEXT_DOCUMENT_SYNC_FULL)
        {
          g_autoptr(JsonNode) params = NULL;
          g_autoptr(GBytes) content = NULL;
          g_autofree char *uri = NULL;
          const char *text;
          gint64 version;

          uri = foundry_text_document_dup_uri (document);
          version = (int)foundry_text_buffer_get_change_count (buffer);

          content = foundry_text_buffer_dup_contents (buffer);
          text = (const char *)g_bytes_get_data (content, NULL);

          params = FOUNDRY_JSON_OBJECT_NEW (
            "textDocument", "{",
              "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
              "version", FOUNDRY_JSON_NODE_PUT_INT (version),
            "}",
            "contentChanges", "[",
              "{",
                "text", FOUNDRY_JSON_NODE_PUT_STRING (text),
              "}",
            "]");

          dex_future_disown (foundry_lsp_client_notify (self, "textDocument/didChange", params));
        }
    }
}

static void
foundry_lsp_client_document_added (FoundryLspClient    *self,
                                   GFile               *file,
                                   FoundryTextDocument *document)
{
  g_autoptr(FoundryTextBuffer) buffer = NULL;
  g_autoptr(JsonNode) params = NULL;
  g_autoptr(GBytes) contents = NULL;
  g_autofree char *language_id = NULL;
  g_autofree char *uri = NULL;
  CommitNotify *notify;
  const char *text;
  gint64 change_count;

  g_assert (FOUNDRY_IS_LSP_CLIENT (self));
  g_assert (G_IS_FILE (file));
  g_assert (FOUNDRY_IS_TEXT_DOCUMENT (document));

  /* TODO: Currently we send all documents to all active LSPs which is
   *       probably fine but also more work than we really need to do.
   *       We could check supports_language() first, but then we need
   *       to track when a file changes its discovered language-id.
   */

  buffer = foundry_text_document_dup_buffer (document);
  change_count = foundry_text_buffer_get_change_count (buffer);
  contents = foundry_text_buffer_dup_contents (buffer);
  language_id = foundry_text_buffer_dup_language_id (buffer);
  uri = foundry_text_document_dup_uri (document);

  if (foundry_str_empty0 (language_id))
    g_set_str (&language_id, "text/plain");

  language_id = translate_language_id (language_id);

  /* contents is \0 terminated */
  text = (const char *)g_bytes_get_data (contents, NULL);

  params = FOUNDRY_JSON_OBJECT_NEW (
    "textDocument", "{",
      "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
      "languageId", FOUNDRY_JSON_NODE_PUT_STRING (language_id),
      "text", FOUNDRY_JSON_NODE_PUT_STRING (text),
      "version", FOUNDRY_JSON_NODE_PUT_INT (change_count),
    "}"
  );

  g_hash_table_replace (self->diagnostics,
                        g_file_dup (file),
                        g_list_store_new (FOUNDRY_TYPE_DIAGNOSTIC));

  /* Setup commit notify tracking */
  notify = g_new0 (CommitNotify, 1);
  notify->self = self;
  g_weak_ref_init (&notify->buffer_wr, buffer);
  g_weak_ref_init (&notify->document_wr, document);
  notify->handler_id =
    foundry_text_buffer_add_commit_notify (buffer,
                                           (FOUNDRY_TEXT_BUFFER_NOTIFY_AFTER_INSERT |
                                            FOUNDRY_TEXT_BUFFER_NOTIFY_BEFORE_DELETE |
                                            FOUNDRY_TEXT_BUFFER_NOTIFY_AFTER_DELETE),
                                           foundry_lsp_client_buffer_commit_notify,
                                           notify, NULL);
  g_hash_table_replace (self->commit_notify, g_object_ref (file), notify);

  dex_future_disown (foundry_lsp_client_notify (self, "textDocument/didOpen", params));
}

static void
foundry_lsp_client_document_removed (FoundryLspClient *self,
                                     GFile            *file)
{
  g_autoptr(JsonNode) params = NULL;
  g_autofree char *uri = NULL;

  g_assert (FOUNDRY_IS_LSP_CLIENT (self));
  g_assert (G_IS_FILE (file));

  uri = g_file_get_uri (file);

  g_hash_table_remove (self->commit_notify, file);
  g_hash_table_remove (self->diagnostics, file);

  params = FOUNDRY_JSON_OBJECT_NEW (
    "textDocument", "{",
      "uri", FOUNDRY_JSON_NODE_PUT_STRING (uri),
    "}"
  );

  dex_future_disown (foundry_lsp_client_notify (self, "textDocument/didClose", params));
}

static DexFuture *
foundry_lsp_client_load_fiber (gpointer data)
{
  FoundryLspClient *self = data;
  g_autoptr(FoundryTextManager) text_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(JsonNode) initialize_params = NULL;
  g_autoptr(JsonNode) initialization_options = NULL;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_dir = NULL;
  g_autofree char *root_uri = NULL;
  g_autofree char *root_path = NULL;
  g_autofree char *basename = NULL;
  const char *trace_string = "off";
  JsonNode *caps = NULL;
  gint64 tds = 0;

  g_assert (FOUNDRY_IS_LSP_CLIENT (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  text_manager = foundry_context_dup_text_manager (context);
  project_dir = foundry_context_dup_project_directory (context);
  root_uri = g_file_get_uri (project_dir);
  basename = g_file_get_basename (project_dir);

  if (g_file_is_native (project_dir))
    root_path = g_file_get_path (project_dir);
  else
    root_path = g_strdup ("");

  if (self->provider != NULL)
    initialization_options = foundry_lsp_provider_dup_initialization_options (self->provider);

  initialize_params = FOUNDRY_JSON_OBJECT_NEW (
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
    "processId", FOUNDRY_JSON_NODE_PUT_INT (getpid ()),
#endif
    "rootUri", FOUNDRY_JSON_NODE_PUT_STRING (root_uri),
    "clientInfo", "{",
      "name", FOUNDRY_JSON_NODE_PUT_STRING ("Foundry"),
      "version", FOUNDRY_JSON_NODE_PUT_STRING (PACKAGE_VERSION),
    "}",
    "rootPath", FOUNDRY_JSON_NODE_PUT_STRING (root_path),
    "workspaceFolders", "[",
      "{",
        "uri", FOUNDRY_JSON_NODE_PUT_STRING (root_uri),
        "name", FOUNDRY_JSON_NODE_PUT_STRING (basename),
      "}",
    "]",
    "trace", FOUNDRY_JSON_NODE_PUT_STRING (trace_string),
    "capabilities", "{",
      "workspace", "{",
        "applyEdit", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
        "configuration", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
        "symbol", "{",
          "SymbolKind", "{",
            "valueSet", "[",
              FOUNDRY_JSON_NODE_PUT_INT (1), /* File */
              FOUNDRY_JSON_NODE_PUT_INT (2), /* Module */
              FOUNDRY_JSON_NODE_PUT_INT (3), /* Namespace */
              FOUNDRY_JSON_NODE_PUT_INT (4), /* Package */
              FOUNDRY_JSON_NODE_PUT_INT (5), /* Class */
              FOUNDRY_JSON_NODE_PUT_INT (6), /* Method */
              FOUNDRY_JSON_NODE_PUT_INT (7), /* Property */
              FOUNDRY_JSON_NODE_PUT_INT (8), /* Field */
              FOUNDRY_JSON_NODE_PUT_INT (9), /* Constructor */
              FOUNDRY_JSON_NODE_PUT_INT (10), /* Enum */
              FOUNDRY_JSON_NODE_PUT_INT (11), /* Interface */
              FOUNDRY_JSON_NODE_PUT_INT (12), /* Function */
              FOUNDRY_JSON_NODE_PUT_INT (13), /* Variable */
              FOUNDRY_JSON_NODE_PUT_INT (14), /* Constant */
              FOUNDRY_JSON_NODE_PUT_INT (15), /* String */
              FOUNDRY_JSON_NODE_PUT_INT (16), /* Number */
              FOUNDRY_JSON_NODE_PUT_INT (17), /* Boolean */
              FOUNDRY_JSON_NODE_PUT_INT (18), /* Array */
              FOUNDRY_JSON_NODE_PUT_INT (19), /* Object */
              FOUNDRY_JSON_NODE_PUT_INT (20), /* Key */
              FOUNDRY_JSON_NODE_PUT_INT (21), /* Null */
              FOUNDRY_JSON_NODE_PUT_INT (22), /* EnumMember */
              FOUNDRY_JSON_NODE_PUT_INT (23), /* Struct */
              FOUNDRY_JSON_NODE_PUT_INT (24), /* Event */
              FOUNDRY_JSON_NODE_PUT_INT (25), /* Operator */
              FOUNDRY_JSON_NODE_PUT_INT (26), /* TypeParameter */
            "]",
          "}",
        "}",
      "}",
      "textDocument", "{",
        "completion", "{",
          "contextSupport", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
          "completionItem", "{",
            "snippetSupport", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
            "documentationFormat", "[",
              "markdown",
              "plaintext",
            "]",
            "deprecatedSupport", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
            "labelDetailsSupport", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
          "}",
          "completionItemKind", "{",
            "valueSet", "[",
              FOUNDRY_JSON_NODE_PUT_INT (1),
              FOUNDRY_JSON_NODE_PUT_INT (2),
              FOUNDRY_JSON_NODE_PUT_INT (3),
              FOUNDRY_JSON_NODE_PUT_INT (4),
              FOUNDRY_JSON_NODE_PUT_INT (5),
              FOUNDRY_JSON_NODE_PUT_INT (6),
              FOUNDRY_JSON_NODE_PUT_INT (7),
              FOUNDRY_JSON_NODE_PUT_INT (8),
              FOUNDRY_JSON_NODE_PUT_INT (9),
              FOUNDRY_JSON_NODE_PUT_INT (10),
              FOUNDRY_JSON_NODE_PUT_INT (11),
              FOUNDRY_JSON_NODE_PUT_INT (12),
              FOUNDRY_JSON_NODE_PUT_INT (13),
              FOUNDRY_JSON_NODE_PUT_INT (14),
              FOUNDRY_JSON_NODE_PUT_INT (15),
              FOUNDRY_JSON_NODE_PUT_INT (16),
              FOUNDRY_JSON_NODE_PUT_INT (17),
              FOUNDRY_JSON_NODE_PUT_INT (18),
              FOUNDRY_JSON_NODE_PUT_INT (19),
              FOUNDRY_JSON_NODE_PUT_INT (20),
              FOUNDRY_JSON_NODE_PUT_INT (21),
              FOUNDRY_JSON_NODE_PUT_INT (22),
              FOUNDRY_JSON_NODE_PUT_INT (23),
              FOUNDRY_JSON_NODE_PUT_INT (24),
              FOUNDRY_JSON_NODE_PUT_INT (25),
            "]",
          "}",
        "}",
        "documentSymbol", "{",
          "hierarchicalDocumentSymbolSupport", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
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
              FOUNDRY_JSON_NODE_PUT_INT (1),
              FOUNDRY_JSON_NODE_PUT_INT (2),
            "]",
          "}",
        "}",
        "codeAction", "{",
          "dynamicRegistration", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
          "isPreferredSupport", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
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
        "workDoneProgress", FOUNDRY_JSON_NODE_PUT_BOOLEAN (TRUE),
      "}",
    "}",
    "initializationOptions", FOUNDRY_JSON_NODE_PUT_NODE (initialization_options)
  );

  if (!(reply = dex_await_boxed (foundry_lsp_client_call (self, "initialize", initialize_params), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (FOUNDRY_JSON_OBJECT_PARSE (reply, "capabilities", FOUNDRY_JSON_NODE_GET_NODE (&caps)))
    self->capabilities = json_node_ref (caps);

  if (FOUNDRY_JSON_OBJECT_PARSE (caps, "textDocumentSync", FOUNDRY_JSON_NODE_GET_INT (&tds)) ||
      FOUNDRY_JSON_OBJECT_PARSE (caps,
                                 "textDocumentSync", "{",
                                   "change", FOUNDRY_JSON_NODE_GET_INT (&tds),
                                 "}"))
    self->text_document_sync = tds & 0x3;
  else
    self->text_document_sync = TEXT_DOCUMENT_SYNC_INCREMENTAL;

  g_signal_connect_object (text_manager,
                           "document-added",
                           G_CALLBACK (foundry_lsp_client_document_added),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (text_manager,
                           "document-removed",
                           G_CALLBACK (foundry_lsp_client_document_removed),
                           self,
                           G_CONNECT_SWAPPED);

  /* Notify LSP of open documents */
  if (dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (text_manager)), NULL))
    {
      g_autoptr(GListModel) documents = foundry_text_manager_list_documents (text_manager);
      guint n_items = g_list_model_get_n_items (documents);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(FoundryTextDocument) document = g_list_model_get_item (documents, i);
          g_autoptr(GFile) file = foundry_text_document_dup_file (document);

          foundry_lsp_client_document_added (self, file, document);
        }
    }

  return dex_future_new_true ();
}

DexFuture *
foundry_lsp_client_new (FoundryContext *context,
                        GIOStream      *io_stream,
                        GSubprocess    *subprocess)
{
  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));
  dex_return_error_if_fail (G_IS_IO_STREAM (io_stream));
  dex_return_error_if_fail (!subprocess || G_IS_SUBPROCESS (subprocess));

  return foundry_lsp_client_new_with_provider (context, io_stream, subprocess, NULL);
}

DexFuture *
foundry_lsp_client_new_with_provider (FoundryContext     *context,
                                      GIOStream          *io_stream,
                                      GSubprocess        *subprocess,
                                      FoundryLspProvider *provider)
{
  g_autoptr(FoundryLspClient) client = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_CONTEXT (context));
  dex_return_error_if_fail (G_IS_IO_STREAM (io_stream));
  dex_return_error_if_fail (!subprocess || G_IS_SUBPROCESS (subprocess));
  dex_return_error_if_fail (!provider || FOUNDRY_IS_LSP_PROVIDER (provider));

  client = g_object_new (FOUNDRY_TYPE_LSP_CLIENT,
                         "context", context,
                         "io-stream", io_stream,
                         "provider", provider,
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

  /* If we don't have anything to check, just assume yes.
   * We may need to reasses this later though depending
   * how it is getting used.
   */
  if (self->provider == NULL)
    return TRUE;

  if ((plugin_info = foundry_lsp_provider_dup_plugin_info (self->provider)))
    {
      const char *x_languages = peas_plugin_info_get_external_data (plugin_info, "LSP-Languages");
      g_auto(GStrv) languages = g_strsplit (x_languages, ";", 0);

      for (guint i = 0; languages[i]; i++)
        {
          if (strcmp (languages[i], language_id) == 0)
            return TRUE;
        }
    }

  return FALSE;
}

GListModel *
_foundry_lsp_client_get_diagnostics (FoundryLspClient *self,
                                     GFile            *file)
{
  g_return_val_if_fail (FOUNDRY_IS_LSP_CLIENT (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return g_hash_table_lookup (self->diagnostics, file);
}
