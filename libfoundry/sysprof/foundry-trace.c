/*
 * foundry-trace.c
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

#include "config.h"

#include <stdbool.h>
#include <stdarg.h>

#if HAVE_SYSPROF
# include <sysprof-collector.h>
# include <sysprof-capture-types.h>
#endif

#include "foundry-trace-private.h"

struct _FoundryTraceScope
{
  gint64 begin_time;
  char *name;
  char *message;
};

#if HAVE_SYSPROF
static gsize trace_initialized;
static gboolean trace_active;
#endif

static gboolean
foundry_trace_ensure_active (void)
{
#if HAVE_SYSPROF
  sysprof_collector_init ();

  if (!sysprof_collector_is_active ())
    return FALSE;

  if (g_once_init_enter (&trace_initialized))
    {
      trace_active = TRUE;
      g_once_init_leave (&trace_initialized, 1);
    }

  return trace_active;
#else
  return FALSE;
#endif
}

#ifdef G_HAS_CONSTRUCTORS
# ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#  pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS (foundry_trace_pre_init)
# endif
G_DEFINE_CONSTRUCTOR (foundry_trace_pre_init)
#endif

static void foundry_trace_pre_init (void) G_GNUC_UNUSED;

static void
foundry_trace_pre_init (void)
{
  foundry_trace_ensure_active ();
}

gboolean
_foundry_trace_is_active (void)
{
  return foundry_trace_ensure_active ();
}

gint64
_foundry_trace_now (void)
{
#if HAVE_SYSPROF
  return SYSPROF_CAPTURE_CURRENT_TIME;
#else
  return g_get_monotonic_time ();
#endif
}

void
_foundry_trace_mark_duration (gint64      start_time,
                              gint64      duration,
                              const char *name,
                              const char *message_format,
                              ...)
{
#if HAVE_SYSPROF
  va_list args;

  g_return_if_fail (name != NULL);

  if (!foundry_trace_ensure_active ())
    return;

  if (message_format == NULL)
    {
      sysprof_collector_mark (start_time, duration, FOUNDRY_TRACE_GROUP, name, "");
      return;
    }

  va_start (args, message_format);
  sysprof_collector_mark_vprintf (start_time,
                                  duration,
                                  FOUNDRY_TRACE_GROUP,
                                  name,
                                  message_format,
                                  args);
  va_end (args);
#endif
}

void
_foundry_trace_mark (const char *name,
                     const char *message_format,
                     ...)
{
#if HAVE_SYSPROF
  va_list args;

  g_return_if_fail (name != NULL);

  if (!foundry_trace_ensure_active ())
    return;

  if (message_format == NULL)
    {
      sysprof_collector_mark (_foundry_trace_now (), 0, FOUNDRY_TRACE_GROUP, name, "");
      return;
    }

  va_start (args, message_format);
  sysprof_collector_mark_vprintf (_foundry_trace_now (),
                                  0,
                                  FOUNDRY_TRACE_GROUP,
                                  name,
                                  message_format,
                                  args);
  va_end (args);
#endif
}

FoundryTraceScope *
_foundry_trace_scope_new (const char *name,
                          const char *message_format,
                          ...)
{
  FoundryTraceScope *scope;

  g_return_val_if_fail (name != NULL, NULL);

  if (!foundry_trace_ensure_active ())
    return NULL;

  scope = g_new0 (FoundryTraceScope, 1);
  scope->begin_time = _foundry_trace_now ();
  scope->name = g_strdup (name);

  if (message_format != NULL)
    {
      va_list args;

      va_start (args, message_format);
      scope->message = g_strdup_vprintf (message_format, args);
      va_end (args);
    }

  return scope;
}

void
_foundry_trace_scope_free (FoundryTraceScope *scope)
{
  if (scope == NULL)
    return;

  _foundry_trace_mark_duration (scope->begin_time,
                                _foundry_trace_now () - scope->begin_time,
                                scope->name,
                                "%s",
                                scope->message != NULL ? scope->message : "");

  g_clear_pointer (&scope->message, g_free);
  g_clear_pointer (&scope->name, g_free);
  g_free (scope);
}
