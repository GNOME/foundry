/* foundry-shortcut-bundle-private.h
 *
 * Copyright 2022-2025 Christian Hergert <chergert@redhat.com>
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

#include <foundry.h>
#include <gtk/gtk.h>
#include <tmpl-glib.h>

G_BEGIN_DECLS

#define FOUNDRY_TYPE_SHORTCUT_BUNDLE (foundry_shortcut_bundle_get_type())

typedef struct _FoundryShortcut
{
  char                *id;
  char                *override;
  GtkShortcutTrigger  *trigger;
  TmplExpr            *when;
  GVariant            *args;
  GtkShortcutAction   *action;
  GtkPropagationPhase  phase;
} FoundryShortcut;

G_DECLARE_FINAL_TYPE (FoundryShortcutBundle, foundry_shortcut_bundle, FOUNDRY, SHORTCUT_BUNDLE, GObject)

FoundryShortcutBundle *foundry_shortcut_bundle_new               (void);
FoundryShortcutBundle *foundry_shortcut_bundle_new_for_user      (GFile                  *file);
gboolean               foundry_shortcut_bundle_parse             (FoundryShortcutBundle  *self,
                                                                  GFile                  *file,
                                                                  GError                **error);
const GError          *foundry_shortcut_bundle_error             (FoundryShortcutBundle  *self);
gboolean               foundry_shortcut_bundle_override          (FoundryShortcutBundle  *bundle,
                                                                  const char             *shortcut_id,
                                                                  const char             *accelerator,
                                                                  GError                **error);
gboolean               foundry_shortcut_is_phase                 (GtkShortcut            *shortcut,
                                                                  GtkPropagationPhase     phase);
gboolean               foundry_shortcut_is_suppress              (GtkShortcut            *shortcut);
void                   foundry_shortcut_bundle_override_triggers (FoundryShortcutBundle  *self,
                                                                  GHashTable             *id_to_trigger);


G_END_DECLS
