/* ssh-agent-sign.c
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

#include <errno.h>
#include <string.h>

#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>

#include "ssh-agent-sign.h"

#define SSH_AGENT_FAILURE        5
#define SSH2_AGENTC_SIGN_REQUEST 13
#define SSH2_AGENT_SIGN_RESPONSE 14

static gboolean
recv_all (GSocket  *socket,
          guint8   *buf,
          gsize     len,
          GError  **error)
{
  gsize off = 0;

  while (off < len) {
    gssize n = g_socket_receive (socket, (char *)buf + off, len - off, NULL, error);

    if (n < 0)
      return FALSE;

    if (n == 0)
      {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE,
                     "SSH agent closed connection unexpectedly");
        return FALSE;
      }

    off += (gsize) n;
  }

  return TRUE;
}

static gboolean
send_all (GSocket       *socket,
          const guint8  *buf,
          gsize          len,
          GError       **error)
{
    gsize off = 0;

    while (off < len)
      {
        gssize n = g_socket_send (socket, (const char *)buf + off,
                                  len - off, NULL, error);
        if (n < 0)
          return FALSE;

        if (n == 0)
          {
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE,
                         "SSH agent closed connection unexpectedly");
            return FALSE;
          }

        off += (gsize) n;
      }

    return TRUE;
}

static void
byte_array_append_u32 (GByteArray *ba,
                       guint32     v)
{
  guint32 be = GINT32_TO_BE (v);
  g_byte_array_append (ba, (const guint8 *)&be, 4);
}

static void
byte_array_append_byte (GByteArray *ba,
                        guint8      b)
{
  g_byte_array_append (ba, &b, 1);
}

static void
byte_array_append_string (GByteArray   *ba,
                          const guint8 *data,
                          gsize         len)
{
  g_assert (len <= G_MAXUINT32);

  byte_array_append_u32 (ba, (guint32)len);

  if (len > 0)
    g_byte_array_append (ba, data, len);
}

static gboolean
parse_ssh_string (const guint8  *buf,
                  gsize          buf_len,
                  gsize         *offset,
                  const guint8 **out_data,
                  gsize         *out_len,
                  GError       **error)
{
  guint32 be_len;
  guint32 len;

  if (*offset + 4 > buf_len)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "Short read while parsing SSH string length");
      return FALSE;
    }

  memcpy (&be_len, buf + *offset, 4);
  *offset += 4;

  len = GINT32_FROM_BE (be_len);

  if (*offset + len > buf_len)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "SSH string length out of bounds");
      return FALSE;
    }

  *out_data = buf + *offset;
  *out_len = len;
  *offset += len;

  return TRUE;
}

/**
 * ssh_agent_sign_data_for_pubkey:
 * @pubkey_line: full ssh public key line, e.g.
 *   "ssh-ed25519 AAAAC3... user@example.com"
 * @data: (array length=data_len): data to sign
 * @data_len: size of @data
 * @out_sig: (out) (transfer full): raw SSH agent signature blob (GBytes)
 * @error: return location for a GError, or NULL
 *
 * Returns: TRUE on success, FALSE on error.
 */
gboolean
ssh_agent_sign_data_for_pubkey (const char    *pubkey_line,
                                const guint8  *data,
                                gsize          data_len,
                                GBytes       **out_sig,
                                GError       **error)
{
  g_autoptr(GSocket) socket = NULL;
  g_autoptr(GSocketAddress) addr = NULL;
  const char *sock_path;
  g_auto(GStrv) tokens = NULL;
  G_GNUC_UNUSED const char *algo;
  const char *b64;
  gsize blob_len = 0;
  g_autofree guint8 *blob = NULL;
  g_autoptr(GByteArray) payload = NULL;
  g_autoptr(GByteArray) packet = NULL;
  g_autofree guint8 *resp = NULL;
  guint8 len_buf[4];
  guint32 be_len;
  guint32 resp_len;
  guint8 rtype;
  gsize offset;
  const guint8 *sig_data = NULL;
  gsize sig_len;

  g_return_val_if_fail (pubkey_line != NULL, FALSE);
  g_return_val_if_fail (data != NULL || data_len == 0, FALSE);
  g_return_val_if_fail (out_sig != NULL, FALSE);

  *out_sig = NULL;

  sock_path = g_getenv ("SSH_AUTH_SOCK");
  if (!sock_path || *sock_path == '\0')
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "SSH_AUTH_SOCK is not set; no ssh-agent available");
      return FALSE;
    }

  /* 1. Parse public key line: "algo base64 [comment]" */
  tokens = g_strsplit_set (pubkey_line, " \t", 3);
  if (!tokens[0] || !tokens[1])
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   "Invalid SSH public key line: '%s'", pubkey_line);
      return FALSE;
    }

  algo = tokens[0]; /* e.g. "ssh-ed25519" */
  b64 = tokens[1];  /* base64 blob */

  blob = g_base64_decode (b64, &blob_len);
  if (!blob || blob_len == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "Failed to base64-decode SSH public key blob");
      return FALSE;
    }

  /* 2. Connect to ssh-agent via UNIX socket */
  addr = g_unix_socket_address_new (sock_path);
  socket = g_socket_new (G_SOCKET_FAMILY_UNIX,
                         G_SOCKET_TYPE_STREAM,
                         G_SOCKET_PROTOCOL_DEFAULT,
                         error);
  if (!socket)
    return FALSE;

  if (!g_socket_connect (socket, addr, NULL, error))
    return FALSE;

  /* 3. Build SSH2_AGENTC_SIGN_REQUEST payload */
  payload = g_byte_array_new ();
  byte_array_append_byte (payload, SSH2_AGENTC_SIGN_REQUEST);

  /* string: key_blob */
  byte_array_append_string (payload, blob, blob_len);

  /* string: data to sign */
  byte_array_append_string (payload, data, data_len);

  /* uint32: flags (0 for ed25519 / basic case) */
  byte_array_append_u32 (payload, 0);

  /* 4. Wrap in length prefix */
  packet = g_byte_array_new ();

  if (payload->len > G_MAXUINT32)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Payload too large for ssh-agent request");
      return FALSE;
    }

  byte_array_append_u32 (packet, (guint32)payload->len);
  g_byte_array_append (packet, payload->data, payload->len);

  /* 5. Send request */
  if (!send_all (socket, packet->data, packet->len, error))
    return FALSE;

  /* 6. Read response length */
  if (!recv_all (socket, len_buf, sizeof len_buf, error))
    return FALSE;

  memcpy (&be_len, len_buf, 4);
  resp_len = GINT32_FROM_BE (be_len);

  if (resp_len == 0 || resp_len > (16 * 1024 * 1024))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "ssh-agent response length %u is invalid", resp_len);
      return FALSE;
    }

  /* 7. Read response payload */
  resp = g_malloc (resp_len);
  if (!recv_all (socket, resp, resp_len, error))
    return FALSE;

  rtype = resp[0];

  if (rtype == SSH_AGENT_FAILURE)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "ssh-agent returned failure for sign request");
      return FALSE;
    }

  if (rtype != SSH2_AGENT_SIGN_RESPONSE)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Unexpected ssh-agent response type %u", rtype);
      return FALSE;
    }

  /* 8. Parse signature "string" from response body */
  offset = 1;
  sig_len = 0;

  if (!parse_ssh_string (resp, resp_len, &offset, &sig_data, &sig_len, error))
    return FALSE;

  /* sig_data is the raw SSH signature blob: string algo; string signature */
  *out_sig = g_bytes_new (sig_data, sig_len);

  return TRUE;
}
