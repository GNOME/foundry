/* foundry-run-manager.c
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

#include "foundry-service-private.h"
#include "foundry-run-manager.h"

struct _FoundryRunManager
{
  FoundryService parent_instance;
};

struct _FoundryRunManagerClass
{
  FoundryServiceClass parent_class;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryRunManager, foundry_run_manager, FOUNDRY_TYPE_SERVICE)

static GParamSpec *properties[N_PROPS];

static void
foundry_run_manager_finalize (GObject *object)
{
  FoundryRunManager *self = (FoundryRunManager *)object;

  G_OBJECT_CLASS (foundry_run_manager_parent_class)->finalize (object);
}

static void
foundry_run_manager_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryRunManager *self = FOUNDRY_RUN_MANAGER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_run_manager_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  FoundryRunManager *self = FOUNDRY_RUN_MANAGER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_run_manager_class_init (FoundryRunManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_run_manager_finalize;
  object_class->get_property = foundry_run_manager_get_property;
  object_class->set_property = foundry_run_manager_set_property;
}

static void
foundry_run_manager_init (FoundryRunManager *self)
{
}

/**
 * foundry_run_manager_list_tools:
 * @self: a #FoundryRunManager
 *
 * Gets the available tools that can be used to run the program.
 *
 * Returns: (transfer full): a list of tools supported by the run manager
 *   such as "gdb" or "valgrind" or "sysprof".
 */
char **
foundry_run_manager_list_tools (FoundryRunManager *self)
{
  g_return_val_if_fail (FOUNDRY_IS_RUN_MANAGER (self), NULL);

  return NULL;
}

/**
 * foundry_run_manager_run:
 * @self: a #FoundryRunManager
 *
 * Starts running a program.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Foundry.RunTool].
 */
DexFuture *
foundry_run_manager_run (FoundryRunManager    *self,
                         FoundryBuildPipeline *pipeilne,
                         FoundryCommand       *command,
                         const char           *tool)
{
  return NULL;
}
