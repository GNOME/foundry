/* plugin-gitlab-error.c
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

#include <foundry.h>

#include "plugin-gitlab-error.h"

gboolean
plugin_gitlab_error_extract (SoupMessage  *message,
                             JsonNode     *node,
                             GError      **error)
{
  SoupStatus status = soup_message_get_status (message);
  const char *errmsg = NULL;

  if (status >= 200 && status <= 299)
    return FALSE;

  if (FOUNDRY_JSON_OBJECT_PARSE (node, "message", FOUNDRY_JSON_NODE_GET_STRING (&errmsg)))
    g_set_error_literal (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         errmsg);
  else
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_FAILED,
                 "Received HTTP code %u", status);

  return TRUE;
}
