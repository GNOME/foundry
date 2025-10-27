/* foundry-intent-manager.c
 *
 * Copyright 2025 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-extension-set.h"
#include "foundry-intent.h"
#include "foundry-intent-handler.h"
#include "foundry-intent-manager.h"
#include "foundry-util.h"

/**
 * FoundryIntentManager:
 *
 * The intent manager provides a generic mechanism to handle intents
 * within an application.
 *
 * This could include opening a file with the OpeFileIntent, or other
 * custom intents.
 *
 * [class@Foundry.IntentHandler] may be registered to handle these
 * intents or ignore them for the next handler to handle them.
 *
 * Since: 1.1
 */

struct _FoundryIntentManager
{
  FoundryService parent_instance;
};

G_DEFINE_FINAL_TYPE (FoundryIntentManager, foundry_intent_manager, FOUNDRY_TYPE_SERVICE)

static void
foundry_intent_manager_class_init (FoundryIntentManagerClass *klass)
{
}

static void
foundry_intent_manager_init (FoundryIntentManager *self)
{
}

static void
collect_addins_cb (FoundryExtensionSet *set,
                   PeasPluginInfo      *plugin_info,
                   GObject             *extension,
                   gpointer             user_data)
{
  GPtrArray *collect = user_data;
  g_ptr_array_add (collect, g_object_ref (extension));
}

static DexFuture *
foundry_intent_manager_dispatch_fiber (FoundryIntentManager *self,
                                       FoundryIntent        *intent)
{
  g_autoptr(FoundryExtensionSet) addins = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) collect = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_INTENT_MANAGER (self));
  g_assert (FOUNDRY_IS_INTENT (intent));

  if (!(context = foundry_contextual_acquire (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  /* Collect all addins first so that plugin unloading will not
   * affect the runtime operation of our async awaits.
   */
  collect = g_ptr_array_new_with_free_func (g_object_unref);
  addins = foundry_extension_set_new (context,
                                      peas_engine_get_default (),
                                      FOUNDRY_TYPE_INTENT_HANDLER,
                                      "Intent-Handler", "*", NULL);
  foundry_extension_set_foreach_by_priority (addins, collect_addins_cb, collect);
  g_clear_object (&addins);

  g_debug ("Trying intent with %u handlers", collect->len);

  if (collect->len == 0)
    return foundry_future_new_not_supported ();

  /* Dispatch to handlers by priority. */
  for (guint i = 0; i < collect->len; i++)
    {
      FoundryIntentHandler *handler = g_ptr_array_index (collect, i);
      g_autoptr(GListModel) results = NULL;
      g_autoptr(DexFuture) future = NULL;
      g_autoptr(GError) dispatch_error = NULL;

      g_debug ("Trying intent with `%s`", G_OBJECT_TYPE_NAME (handler));

      future = foundry_intent_handler_dispatch (handler, intent);
      if (dex_await (dex_ref (future), &dispatch_error))
        return dex_ref (future);

      if (error == NULL &&
          !g_error_matches (dispatch_error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        error = g_steal_pointer (&dispatch_error);
    }

  /* Bail with first non-synthetic error */
  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return foundry_future_new_not_supported ();
}

/**
 * foundry_intent_manager_dispatch:
 * @self: a [class@Foundry.IntentManager]
 * @intent: a [class@Foundry.Intent]
 *
 * Dispatch the intent to the first handler which can handle it.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   if sucessful or rejects with error.
 *
 * Since: 1.1
 */
DexFuture *
foundry_intent_manager_dispatch (FoundryIntentManager *self,
                                 FoundryIntent        *intent)
{
  dex_return_error_if_fail (FOUNDRY_IS_INTENT_MANAGER (self));
  dex_return_error_if_fail (FOUNDRY_IS_INTENT (intent));

  return foundry_scheduler_spawn (NULL, 0,
                                  G_CALLBACK (foundry_intent_manager_dispatch_fiber),
                                  2,
                                  FOUNDRY_TYPE_INTENT_MANAGER, self,
                                  FOUNDRY_TYPE_INTENT, intent);
}
