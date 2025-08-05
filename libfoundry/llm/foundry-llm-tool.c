/* foundry-llm-tool.c
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

#include "foundry-llm-tool.h"

G_DEFINE_ABSTRACT_TYPE (FoundryLlmTool, foundry_llm_tool, G_TYPE_OBJECT)

static void
foundry_llm_tool_finalize (GObject *object)
{
  G_OBJECT_CLASS (foundry_llm_tool_parent_class)->finalize (object);
}

static void
foundry_llm_tool_class_init (FoundryLlmToolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_llm_tool_finalize;
}

static void
foundry_llm_tool_init (FoundryLlmTool *self)
{
}
