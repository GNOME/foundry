/*
 * foundry-trace-private.h
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define FOUNDRY_TRACE_GROUP "Foundry"

typedef struct _FoundryTraceScope FoundryTraceScope;

gboolean            _foundry_trace_is_active           (void);
gint64              _foundry_trace_now                 (void);
void                _foundry_trace_mark                (const char          *name,
                                                        const char          *message_format,
                                                        ...) G_GNUC_PRINTF (2, 3);
void                _foundry_trace_mark_duration       (gint64               start_time,
                                                        gint64               duration,
                                                        const char          *name,
                                                        const char          *message_format,
                                                        ...) G_GNUC_PRINTF (4, 5);
FoundryTraceScope  *_foundry_trace_scope_new           (const char          *name,
                                                        const char          *message_format,
                                                        ...) G_GNUC_PRINTF (2, 3);
void                _foundry_trace_scope_free          (FoundryTraceScope   *scope);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FoundryTraceScope, _foundry_trace_scope_free)

#define FOUNDRY_TRACE_SCOPE(name, ...) \
  _Pragma ("GCC diagnostic push") \
  _Pragma ("GCC diagnostic ignored \"-Wdeclaration-after-statement\"") \
  g_autoptr(FoundryTraceScope) G_PASTE (__foundry_trace_scope_, __LINE__) = \
    _foundry_trace_scope_new ((name), __VA_ARGS__); \
  _Pragma ("GCC diagnostic pop")

#define FOUNDRY_TRACE_SCOPE_FUNC() \
  FOUNDRY_TRACE_SCOPE (G_STRFUNC, NULL)

#define FOUNDRY_TRACE_BEGIN_MARK() \
  (_foundry_trace_now ())

#define FOUNDRY_TRACE_END_MARK(start_time, name, ...) \
  _foundry_trace_mark_duration ((start_time), _foundry_trace_now () - (start_time), \
                                (name), __VA_ARGS__)

#define FOUNDRY_TRACE_MARK(name, ...) \
  _foundry_trace_mark ((name), __VA_ARGS__)

G_END_DECLS
