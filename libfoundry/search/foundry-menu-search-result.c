/* foundry-menu-search-result.c
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

#include "foundry-action-intent.h"
#include "foundry-menu-search-result.h"

struct _FoundryMenuSearchResult
{
  FoundrySearchResult parent_instance;
  GMenuModel *menu_model;
  guint index;
};

G_DEFINE_FINAL_TYPE (FoundryMenuSearchResult, foundry_menu_search_result, FOUNDRY_TYPE_SEARCH_RESULT)

static char *
foundry_menu_search_result_dup_title (FoundrySearchResult *result)
{
  FoundryMenuSearchResult *self = FOUNDRY_MENU_SEARCH_RESULT (result);
  g_autofree char *label = NULL;

  if (g_menu_model_get_item_attribute (self->menu_model, self->index, "label", "s", &label))
    return g_steal_pointer (&label);

  return g_strdup ("");
}

static char *
foundry_menu_search_result_dup_subtitle (FoundrySearchResult *result)
{
  FoundryMenuSearchResult *self = FOUNDRY_MENU_SEARCH_RESULT (result);
  g_autofree char *description = NULL;

  if (g_menu_model_get_item_attribute (self->menu_model, self->index, "description", "s", &description))
    return g_steal_pointer (&description);

  return NULL;
}

static GIcon *
foundry_menu_search_result_dup_icon (FoundrySearchResult *result)
{
  FoundryMenuSearchResult *self = FOUNDRY_MENU_SEARCH_RESULT (result);
  g_autofree char *verb_icon = NULL;

  if (g_menu_model_get_item_attribute (self->menu_model, self->index, "verb-icon", "s", &verb_icon))
    return g_themed_icon_new (verb_icon);

  return g_themed_icon_new ("action-activate-symbolic");
}

static FoundryIntent *
foundry_menu_search_result_create_intent (FoundrySearchResult *result,
                                          FoundryContext      *context)
{
  FoundryMenuSearchResult *self = FOUNDRY_MENU_SEARCH_RESULT (result);
  g_autofree char *action = NULL;
  g_autoptr(GVariant) target = NULL;

  if (!g_menu_model_get_item_attribute (self->menu_model, self->index, "action", "s", &action))
    return NULL;

  target = g_menu_model_get_item_attribute_value (self->menu_model, self->index, "target", NULL);

  return foundry_action_intent_new (action, target);
}

static void
foundry_menu_search_result_finalize (GObject *object)
{
  FoundryMenuSearchResult *self = FOUNDRY_MENU_SEARCH_RESULT (object);

  g_clear_object (&self->menu_model);

  G_OBJECT_CLASS (foundry_menu_search_result_parent_class)->finalize (object);
}

static void
foundry_menu_search_result_class_init (FoundryMenuSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundrySearchResultClass *search_result_class = FOUNDRY_SEARCH_RESULT_CLASS (klass);

  object_class->finalize = foundry_menu_search_result_finalize;

  search_result_class->dup_title = foundry_menu_search_result_dup_title;
  search_result_class->dup_subtitle = foundry_menu_search_result_dup_subtitle;
  search_result_class->dup_icon = foundry_menu_search_result_dup_icon;
  search_result_class->create_intent = foundry_menu_search_result_create_intent;
}

static void
foundry_menu_search_result_init (FoundryMenuSearchResult *self)
{
}

FoundryMenuSearchResult *
foundry_menu_search_result_new (GMenuModel *menu_model,
                                guint       index)
{
  FoundryMenuSearchResult *self;

  g_return_val_if_fail (G_IS_MENU_MODEL (menu_model), NULL);

  self = g_object_new (FOUNDRY_TYPE_MENU_SEARCH_RESULT, NULL);
  self->menu_model = g_object_ref (menu_model);
  self->index = index;

  return self;
}

GMenuModel *
foundry_menu_search_result_dup_menu_model (FoundryMenuSearchResult *self)
{
  g_return_val_if_fail (FOUNDRY_IS_MENU_SEARCH_RESULT (self), NULL);

  return g_object_ref (self->menu_model);
}

guint
foundry_menu_search_result_get_index (FoundryMenuSearchResult *self)
{
  g_return_val_if_fail (FOUNDRY_IS_MENU_SEARCH_RESULT (self), 0);

  return self->index;
}
