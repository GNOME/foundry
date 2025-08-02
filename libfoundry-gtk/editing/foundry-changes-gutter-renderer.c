/* foundry-changes-gutter-renderer.c
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

#include "foundry-changes-gutter-renderer.h"
#include "foundry-source-buffer.h"
#include "foundry-source-view.h"
#include "foundry-util-private.h"

struct _FoundryChangesGutterRenderer
{
  GtkSourceGutterRenderer  parent_instance;
  FoundryVcsLineChanges   *changes;
  DexFuture               *update_fiber;
};

G_DEFINE_FINAL_TYPE (FoundryChangesGutterRenderer, foundry_changes_gutter_renderer, GTK_SOURCE_TYPE_GUTTER_RENDERER)

static DexFuture *
foundry_changes_gutter_renderer_update_fiber (gpointer data)
{
  GWeakRef *wr = data;

  g_assert (wr != NULL);

  for (;;)
    {
      g_autoptr(FoundryChangesGutterRenderer) self = g_weak_ref_get (wr);
      g_autoptr(FoundryVcsLineChanges) changes = NULL;
      g_autoptr(FoundryTextDocument) document = NULL;
      g_autoptr(FoundryVcsManager) vcs_manager = NULL;
      g_autoptr(FoundryContext) context = NULL;
      g_autoptr(FoundryVcsFile) vcs_file = NULL;
      g_autoptr(FoundryVcs) vcs = NULL;
      g_autoptr(DexFuture) changed = NULL;
      g_autoptr(GBytes) contents = NULL;
      g_autoptr(GFile) file = NULL;
      GtkTextBuffer *buffer;
      GtkSourceView *view;

      if (self == NULL)
        break;

      if (!(view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self))))
        break;

      if (!FOUNDRY_IS_SOURCE_VIEW (view))
        break;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      if (!FOUNDRY_IS_SOURCE_BUFFER (buffer))
        break;

      contents = foundry_text_buffer_dup_contents (FOUNDRY_TEXT_BUFFER (buffer));
      document = foundry_source_view_dup_document (FOUNDRY_SOURCE_VIEW (view));
      file = foundry_text_document_dup_file (document);
      context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (document));
      vcs_manager = foundry_context_dup_vcs_manager (context);

      if (!(vcs = foundry_vcs_manager_dup_vcs (vcs_manager)))
        break;

      changed = foundry_text_document_when_changed (document);

      if (!(vcs_file = dex_await_object (foundry_vcs_find_file (vcs, file), NULL)))
        break;

      if (!(changes = dex_await_object (foundry_vcs_describe_line_changes (vcs, vcs_file, contents), NULL)))
        break;

      if (g_set_object (&self->changes, changes))
        gtk_widget_queue_draw (GTK_WIDGET (self));

      g_clear_object (&self);

      if (!dex_await (g_steal_pointer (&changed), NULL))
        break;
    }

  return dex_future_new_true ();
}

static void
foundry_changes_gutter_renderer_start (FoundryChangesGutterRenderer *self)
{
  g_assert (FOUNDRY_IS_CHANGES_GUTTER_RENDERER (self));

  if (self->update_fiber == NULL)
    self->update_fiber = dex_scheduler_spawn (NULL, 0,
                                              foundry_changes_gutter_renderer_update_fiber,
                                              foundry_weak_ref_new (self),
                                              (GDestroyNotify) foundry_weak_ref_free);
}

static void
foundry_changes_gutter_renderer_dispose (GObject *object)
{
  FoundryChangesGutterRenderer *self = (FoundryChangesGutterRenderer *)object;

  g_clear_object (&self->changes);
  dex_clear (&self->update_fiber);

  G_OBJECT_CLASS (foundry_changes_gutter_renderer_parent_class)->dispose (object);
}

static void
foundry_changes_gutter_renderer_change_buffer (GtkSourceGutterRenderer *renderer,
                                               GtkSourceBuffer         *buffer)
{
  foundry_changes_gutter_renderer_start (FOUNDRY_CHANGES_GUTTER_RENDERER (renderer));
}

static void
foundry_changes_gutter_renderer_class_init (FoundryChangesGutterRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkSourceGutterRendererClass *gutter_renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

  object_class->dispose = foundry_changes_gutter_renderer_dispose;

  gutter_renderer_class->change_buffer = foundry_changes_gutter_renderer_change_buffer;
}

static void
foundry_changes_gutter_renderer_init (FoundryChangesGutterRenderer *self)
{
}

GtkSourceGutterRenderer *
foundry_changes_gutter_renderer_new (void)
{
  return g_object_new (FOUNDRY_TYPE_CHANGES_GUTTER_RENDERER, NULL);
}
