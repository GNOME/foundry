/* foundry-action-muxer-private.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

typedef void (*BuilderActionActivateFunc) (gpointer    instance,
                                           const char *action_name,
                                           GVariant   *param);

typedef struct _BuilderAction
{
  const struct _BuilderAction *next;
  const char                  *name;
  GType                        owner;
  const GVariantType          *parameter_type;
  const GVariantType          *state_type;
  GParamSpec                  *pspec;
  BuilderActionActivateFunc    activate;
  guint                        position;
} BuilderAction;

typedef struct _BuilderActionMixin
{
  GObjectClass        *object_class;
  const BuilderAction *actions;
  guint                n_actions;
} BuilderActionMixin;

#define FOUNDRY_TYPE_ACTION_MUXER (foundry_action_muxer_get_type())

G_DECLARE_FINAL_TYPE (FoundryActionMuxer, foundry_action_muxer, FOUNDRY, ACTION_MUXER, GObject)

FoundryActionMuxer  *builder_action_mixin_get_action_muxer        (gpointer                   instance);
void                 builder_action_mixin_init                    (BuilderActionMixin        *mixin,
                                                                   GObjectClass              *object_class);
void                 builder_action_mixin_constructed             (const BuilderActionMixin  *mixin,
                                                                   gpointer                   instance);
void                 builder_action_mixin_set_enabled             (gpointer                   instance,
                                                                   const char                *action,
                                                                   gboolean                   enabled);
void                 builder_action_mixin_install_action          (BuilderActionMixin        *mixin,
                                                                   const char                *action_name,
                                                                   const char                *parameter_type,
                                                                   BuilderActionActivateFunc  activate);
void                 builder_action_mixin_install_property_action (BuilderActionMixin        *mixin,
                                                                   const char                *action_name,
                                                                   const char                *property_name);
FoundryActionMuxer  *foundry_action_muxer_new                     (void);
void                 foundry_action_muxer_remove_all              (FoundryActionMuxer        *self);
void                 foundry_action_muxer_insert_action_group     (FoundryActionMuxer        *self,
                                                                   const char                *prefix,
                                                                   GActionGroup              *action_group);
void                 foundry_action_muxer_remove_action_group     (FoundryActionMuxer        *self,
                                                                   const char                *prefix);
char               **foundry_action_muxer_list_groups             (FoundryActionMuxer        *self);
GActionGroup        *foundry_action_muxer_get_action_group        (FoundryActionMuxer        *self,
                                                                   const char                *prefix);
void                 foundry_action_muxer_set_enabled             (FoundryActionMuxer        *self,
                                                                   const BuilderAction       *action,
                                                                   gboolean                   enabled);
void                 foundry_action_muxer_connect_actions         (FoundryActionMuxer        *self,
                                                                   gpointer                   instance,
                                                                   const BuilderAction       *actions);

G_END_DECLS

