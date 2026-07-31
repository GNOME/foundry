/* test-redacted-input-stream.c
 *
 * Copyright 2026 Christian Hergert
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

#include "foundry-redacted-input-stream-private.h"

static GBytes *
read_redacted (const guint8       *input,
               gsize               input_len,
               gsize               split,
               gsize               read_size,
               const char * const *secrets)
{
  g_autoptr(GInputStream) base_stream = NULL;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GByteArray) output = NULL;
  g_autoptr(GError) error = NULL;
  guint8 buffer[32];

  g_assert (input != NULL || input_len == 0);
  g_assert_cmpuint (split, <=, input_len);
  g_assert_cmpuint (read_size, >, 0);
  g_assert_cmpuint (read_size, <=, sizeof buffer);
  g_assert (secrets != NULL);

  base_stream = g_memory_input_stream_new ();

  if (split > 0)
    g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (base_stream),
                                    g_memdup2 (input, split),
                                    split,
                                    g_free);

  if (split < input_len)
    g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (base_stream),
                                    g_memdup2 (input + split, input_len - split),
                                    input_len - split,
                                    g_free);

  stream = G_INPUT_STREAM (
    _foundry_redacted_input_stream_new (base_stream, secrets));
  output = g_byte_array_new ();

  for (;;)
    {
      gssize n_read;

      n_read = g_input_stream_read (stream,
                                    buffer,
                                    read_size,
                                    NULL,
                                    &error);
      g_assert_no_error (error);
      g_assert_cmpint (n_read, >=, 0);

      if (n_read == 0)
        break;

      g_byte_array_append (output, buffer, n_read);
    }

  return g_byte_array_free_to_bytes (g_steal_pointer (&output));
}

static void
assert_redacted (const guint8       *input,
                 gsize               input_len,
                 gsize               split,
                 gsize               read_size,
                 const char * const *secrets,
                 const guint8       *expected,
                 gsize               expected_len)
{
  g_autoptr(GBytes) actual = NULL;
  gconstpointer actual_data;
  gsize actual_len;

  actual = read_redacted (input, input_len, split, read_size, secrets);
  actual_data = g_bytes_get_data (actual, &actual_len);

  g_assert_cmpmem (actual_data, actual_len, expected, expected_len);
}

static void
test_redacted_input_stream_boundaries (void)
{
  static const char input[] = "prefix-swordfish-suffix";
  static const char expected[] = "prefix-[REDACTED]-suffix";
  static const char *secrets[] = {"swordfish", NULL};

  for (gsize split = 0; split <= sizeof input - 1; split++)
    {
      for (gsize read_size = 1; read_size <= 16; read_size++)
        assert_redacted ((const guint8 *)input,
                         sizeof input - 1,
                         split,
                         read_size,
                         secrets,
                         (const guint8 *)expected,
                         sizeof expected - 1);
    }
}

static void
test_redacted_input_stream_binary (void)
{
  static const guint8 input[] = {'b', 'e', 'f', 'o', 'r', 'e', 0,
                                 's', 'e', 'c', 'r', 'e', 't',
                                 0, 'a', 'f', 't', 'e', 'r'};
  static const guint8 expected[] = {'b', 'e', 'f', 'o', 'r', 'e', 0,
                                    '[', 'R', 'E', 'D', 'A', 'C', 'T', 'E', 'D', ']',
                                    0, 'a', 'f', 't', 'e', 'r'};
  static const char *secrets[] = {"secret", NULL};

  for (gsize split = 0; split <= sizeof input; split++)
    assert_redacted (input,
                     sizeof input,
                     split,
                     3,
                     secrets,
                     expected,
                     sizeof expected);
}

static void
test_redacted_input_stream_overlapping (void)
{
  static const char input[] = "abcdef abc abx";
  static const char expected[] = "[REDACTED] [REDACTED] [REDACTED]x";
  static const char *secrets[] = {"abc", "abcdef", "ab", "abc", NULL};

  assert_redacted ((const guint8 *)input,
                   sizeof input - 1,
                   4,
                   1,
                   secrets,
                   (const guint8 *)expected,
                   sizeof expected - 1);
}

static void
test_redacted_input_stream_eof (void)
{
  static const char input[] = "secret secrets sec";
  static const char expected[] = "[REDACTED] [REDACTED]s sec";
  static const char *secrets[] = {"secret", NULL};

  assert_redacted ((const guint8 *)input,
                   sizeof input - 1,
                   sizeof input - 4,
                   2,
                   secrets,
                   (const guint8 *)expected,
                   sizeof expected - 1);
}

static void
test_redacted_input_stream_replacement (void)
{
  static const char input[] = "REDACTED";
  static const char expected[] = "[REDACTED]";
  static const char *secrets[] = {"REDACTED", NULL};

  assert_redacted ((const guint8 *)input,
                   sizeof input - 1,
                   3,
                   1,
                   secrets,
                   (const guint8 *)expected,
                   sizeof expected - 1);
}

static void
test_redacted_input_stream_passthrough (void)
{
  static const guint8 input[] = {'a', 0, 'b', 0xff, 'c'};
  static const char *secrets[] = {"", "", NULL};

  assert_redacted (input,
                   sizeof input,
                   2,
                   1,
                   secrets,
                   input,
                   sizeof input);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Foundry/RedactedInputStream/boundaries",
                   test_redacted_input_stream_boundaries);
  g_test_add_func ("/Foundry/RedactedInputStream/binary",
                   test_redacted_input_stream_binary);
  g_test_add_func ("/Foundry/RedactedInputStream/overlapping",
                   test_redacted_input_stream_overlapping);
  g_test_add_func ("/Foundry/RedactedInputStream/eof",
                   test_redacted_input_stream_eof);
  g_test_add_func ("/Foundry/RedactedInputStream/replacement",
                   test_redacted_input_stream_replacement);
  g_test_add_func ("/Foundry/RedactedInputStream/passthrough",
                   test_redacted_input_stream_passthrough);

  return g_test_run ();
}
