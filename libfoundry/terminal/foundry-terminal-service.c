/* foundry-terminal-service.c
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

#include "foundry-command.h"
#include "foundry-intent-manager.h"
#include "foundry-shell.h"
#include "foundry-terminal-intent.h"
#include "foundry-terminal-launcher.h"
#include "foundry-terminal-service.h"
#include "foundry-util.h"

/**
 * FoundryTerminalService:
 *
 * Service providing convenient access to terminal operations.
 */

struct _FoundryTerminalService
{
  FoundryService parent_instance;
};

struct _FoundryTerminalServiceClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryTerminalService, foundry_terminal_service, FOUNDRY_TYPE_SERVICE)

static void
launch_host_terminal_action (FoundryService *service,
                             const char     *action_name,
                             GVariant       *parameter)
{
  FoundryTerminalService *self = FOUNDRY_TERMINAL_SERVICE (service);
  g_autoptr(FoundryTerminalLauncher) launcher = NULL;
  g_autoptr(FoundryCommand) command = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GStrvBuilder) args = NULL;
  g_auto(GStrv) argv = NULL;
  const char *shell = NULL;
  const char *cwd = NULL;

  if (!(context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (service))))
    return;

  shell = foundry_shell_get_default ();
  file = foundry_context_dup_project_directory (context);
  cwd = g_file_peek_path (file);

  args = g_strv_builder_new ();
  g_strv_builder_add (args, shell);
  argv = g_strv_builder_end (args);

  command = foundry_command_new (context);
  foundry_command_set_locality (command, FOUNDRY_COMMAND_LOCALITY_HOST);
  foundry_command_set_argv (command, (const char * const *)argv);
  foundry_command_set_cwd (command, cwd);

  launcher = foundry_terminal_launcher_new (command, NULL);

  foundry_terminal_service_launch (self, launcher);
}

static void
foundry_terminal_service_class_init (FoundryTerminalServiceClass *klass)
{
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);

  foundry_service_class_set_action_prefix (service_class, "terminal");

  foundry_service_class_install_action (service_class, "launch-host-terminal", NULL, launch_host_terminal_action);
}

static void
foundry_terminal_service_init (FoundryTerminalService *self)
{
}

/**
 * foundry_terminal_service_launch:
 * @self: a [class@Foundry.TerminalService]
 *
 * Requests that a new terminal be launched.
 *
 * This function will create a [class@Foundry.TerminalIntent] which the
 * application may handle to show a terminal.
 *
 * Returns: (transfer full): A [class@Dex.Future] that resolves to
 *   any value if succesfull; otherwise rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_terminal_service_launch (FoundryTerminalService  *self,
                                 FoundryTerminalLauncher *launcher)
{
  g_autoptr(FoundryIntentManager) intent_manager = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryIntent) intent = NULL;
  g_autoptr(GError) error = NULL;

  dex_return_error_if_fail (FOUNDRY_IS_TERMINAL_SERVICE (self));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(intent_manager = foundry_context_dup_intent_manager (context)))
    return foundry_future_new_not_supported ();

  intent = foundry_terminal_intent_new (launcher);

  return foundry_intent_manager_dispatch (intent_manager, intent);
}
