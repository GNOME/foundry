/* test-progress-icon.c
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

#include <gtk/gtk.h>

#include "../libfoundry-adw/foundry-progress-icon-private.h"

static GMainLoop *main_loop;

static void
on_close_request (GtkWindow *window)
{
  g_main_loop_quit (main_loop);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(FoundryProgressIcon) progress_icon = NULL;
  GtkAdjustment *adjustment = NULL;
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *picture;
  GtkWidget *scale;

  gtk_init ();

  main_loop = g_main_loop_new (NULL, FALSE);

  progress_icon = foundry_progress_icon_new ();

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 400,
                         "default-height", 300,
                         NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 12,
                      "margin-start", 12,
                      "margin-end", 12,
                      "margin-top", 12,
                      "margin-bottom", 12,
                      NULL);
  gtk_window_set_child (GTK_WINDOW (window), box);

  picture = g_object_new (GTK_TYPE_PICTURE,
                          "paintable", progress_icon,
                          NULL);
  gtk_widget_set_size_request (picture, 256, 256);
  gtk_box_append (GTK_BOX (box), picture);

  adjustment = gtk_adjustment_new (0.0, 0.0, 1.0, 0.01, 0.1, 0.0);
  scale = g_object_new (GTK_TYPE_SCALE,
                        "orientation", GTK_ORIENTATION_VERTICAL,
                        "adjustment", adjustment,
                        "vexpand", TRUE,
                        NULL);
  gtk_box_append (GTK_BOX (box), scale);

  g_object_bind_property (adjustment, "value",
                          progress_icon, "progress",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  g_signal_connect (window, "close-request", G_CALLBACK (on_close_request), NULL);

  gtk_window_present (GTK_WINDOW (window));

  g_main_loop_run (main_loop);

  return 0;
}
