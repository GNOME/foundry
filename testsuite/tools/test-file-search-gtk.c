/* test-file-search-gtk.c
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

#include <foundry.h>
#include <gtk/gtk.h>
#include <pango/pango.h>

#include <glib/gstdio.h>

static GMainLoop *main_loop;
static const char *dirpath;
static const char *search_text;

static void
setup_label (GtkListItemFactory *factory,
             GtkListItem        *list_item)
{
  GtkLabel *label = GTK_LABEL (gtk_label_new (NULL));
  gtk_label_set_xalign (label, 0.0f);
  gtk_list_item_set_child (list_item, GTK_WIDGET (label));
}

static void
bind_string_property (GtkListItemFactory *factory,
                      GtkListItem        *list_item)
{
  GtkLabel *label = GTK_LABEL (gtk_list_item_get_child (list_item));
  FoundryFileSearchMatch *match = FOUNDRY_FILE_SEARCH_MATCH (gtk_list_item_get_item (list_item));
  g_autofree char *text = NULL;

  if (g_strcmp0 (g_object_get_data (G_OBJECT (factory), "property"), "uri") == 0)
    {
      GFile *file = foundry_file_search_match_get_file (match);
      text = g_file_get_uri (file);
      gtk_label_set_text (label, text ? text : "");
    }
  else if (g_strcmp0 (g_object_get_data (G_OBJECT (factory), "property"), "text") == 0)
    {
      text = g_strdup (foundry_file_search_match_get_text (match));
      gtk_label_set_text (label, text ? text : "");

      /* Add Pango highlighting for the matched portion */
      if (text)
        {
          guint line_offset = foundry_file_search_match_get_line_offset (match);
          guint length = foundry_file_search_match_get_length (match);
          g_autoptr(PangoAttrList) attr_list = pango_attr_list_new ();
          PangoAttribute *bg_attr;

          /* Create background color attribute for the matched text */
          bg_attr = pango_attr_background_new (0xE5 * 257, 0xA5 * 257, 0x0A * 257);
          bg_attr->start_index = line_offset;
          bg_attr->end_index = line_offset + length;
          pango_attr_list_insert (attr_list, bg_attr);

          /* Apply the attribute list to the label */
          gtk_label_set_attributes (label, attr_list);
        }
    }
  else
    {
      gtk_label_set_text (label, text ? text : "");
    }
}

static void
bind_line_property (GtkListItemFactory *factory,
                    GtkListItem        *list_item)
{
  GtkLabel *label = GTK_LABEL (gtk_list_item_get_child (list_item));
  FoundryFileSearchMatch *match = FOUNDRY_FILE_SEARCH_MATCH (gtk_list_item_get_item (list_item));
  g_autofree char *text = NULL;

  guint line = foundry_file_search_match_get_line (match);
  text = g_strdup_printf ("%u", line + 1); /* Convert to 1-based line numbers */

  gtk_label_set_text (label, text);
}

static void
on_row_activated (GtkColumnView *columnview,
                  guint          position)
{
  GtkSelectionModel *selection_model;
  FoundryFileSearchMatch *match;
  GFile *file;
  g_autofree char *uri = NULL;

  selection_model = gtk_column_view_get_model (columnview);
  match = FOUNDRY_FILE_SEARCH_MATCH (g_list_model_get_item (G_LIST_MODEL (selection_model), position));

  if (!match)
    return;

  file = foundry_file_search_match_get_file (match);
  uri = g_file_get_uri (file);

  g_print ("Activated match: %s:%u\n", uri, foundry_file_search_match_get_line (match) + 1);
}

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FoundryFileManager) file_manager = NULL;
  g_autoptr(FoundryFileSearchOptions) search_options = NULL;
  g_autoptr(FoundryOperation) operation = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GListModel) results = NULL;
  g_autofree char *path = NULL;
  GtkScrolledWindow *scroller;
  GtkColumnView *columnview;
  GtkWindow *window;
  GtkBox *box;

  dex_await (foundry_init (), NULL);

  if (!(path = dex_await_string (foundry_context_discover (dirpath, NULL), &error)))
    g_error ("%s", error->message);

  if (!(context = dex_await_object (foundry_context_new (path, dirpath, FOUNDRY_CONTEXT_FLAGS_NONE, NULL), &error)))
    g_error ("%s", error->message);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 800,
                         "default-height", 600,
                         "title", "File Search Results",
                         NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      NULL);
  gtk_window_set_child (window, GTK_WIDGET (box));

  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "vexpand", TRUE,
                           NULL);
  gtk_box_append (box, GTK_WIDGET (scroller));

  file_manager = foundry_context_dup_file_manager (context);
  dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (file_manager)), NULL);

  search_options = foundry_file_search_options_new ();
  foundry_file_search_options_set_search_text (search_options, search_text);
  foundry_file_search_options_set_recursive (search_options, TRUE);
  foundry_file_search_options_set_case_sensitive (search_options, FALSE);
  foundry_file_search_options_set_use_regex (search_options, TRUE);

  /* Add current directory as search target */
  {
    g_autoptr(GFile) current_dir = g_file_new_for_path (dirpath);
    foundry_file_search_options_add_target (search_options, current_dir);
  }

  operation = foundry_operation_new ();

  if (!(results = dex_await_object (foundry_file_manager_search (file_manager, search_options, operation), &error)))
    g_error ("Failed to search files: %s", error->message);

  /* Wait for all results to be received */
  if (!dex_await (foundry_list_model_await (results), &error))
    g_error ("Failed to await search results: %s", error->message);

  g_print ("Found %u search results\n", g_list_model_get_n_items (results));

  {
    g_autoptr(GtkSingleSelection) model = gtk_single_selection_new (g_object_ref (G_LIST_MODEL (results)));

    columnview = g_object_new (GTK_TYPE_COLUMN_VIEW,
                               "height-request", 400,
                               "model", model,
                               NULL);
    gtk_scrolled_window_set_child (scroller, GTK_WIDGET (columnview));

    /* Connect row activation signal */
    g_signal_connect (columnview,
                      "activate",
                      G_CALLBACK (on_row_activated),
                      NULL);

    /* Create columns for FoundryFileSearchMatch properties */
    {
      GtkColumnViewColumn *column;
      GtkListItemFactory *factory;

      /* URI column */
      factory = gtk_signal_list_item_factory_new ();
      g_object_set_data (G_OBJECT (factory), "property", (gpointer) "uri");
      g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_string_property), NULL);
      column = gtk_column_view_column_new ("URI", factory);
      gtk_column_view_column_set_expand (column, TRUE);
      gtk_column_view_append_column (columnview, column);

      /* Line column */
      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_line_property), NULL);
      column = gtk_column_view_column_new ("Line", factory);
      gtk_column_view_append_column (columnview, column);

      /* Text column */
      factory = gtk_signal_list_item_factory_new ();
      g_object_set_data (G_OBJECT (factory), "property", (gpointer) "text");
      g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_string_property), NULL);
      column = gtk_column_view_column_new ("Text", factory);
      gtk_column_view_column_set_expand (column, TRUE);
      gtk_column_view_append_column (columnview, column);
    }
  }

  g_signal_connect_swapped (window,
                            "close-request",
                            G_CALLBACK (g_main_loop_quit),
                            main_loop);
  gtk_window_present (window);

  return NULL;
}

int
main (int   argc,
      char *argv[])
{
  if (argc != 3)
    {
      g_printerr ("usage: %s PROJECT_DIR SEARCH_TEXT\n", argv[0]);
      return 1;
    }

  dirpath = argv[1];
  search_text = argv[2];

  gtk_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 8*1024*1024, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
