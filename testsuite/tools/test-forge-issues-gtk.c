#include <foundry.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include <glib/gstdio.h>

static GMainLoop *main_loop;
static const char *dirpath;

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
  FoundryForgeIssue *issue = FOUNDRY_FORGE_ISSUE (gtk_list_item_get_item (list_item));
  g_autofree char *text = NULL;

  if (g_strcmp0 (g_object_get_data (G_OBJECT (factory), "property"), "id") == 0)
    text = foundry_forge_issue_dup_id (issue);
  else if (g_strcmp0 (g_object_get_data (G_OBJECT (factory), "property"), "state") == 0)
    text = foundry_forge_issue_dup_state (issue);
  else if (g_strcmp0 (g_object_get_data (G_OBJECT (factory), "property"), "title") == 0)
    text = foundry_forge_issue_dup_title (issue);

  gtk_label_set_text (label, text ? text : "");
}

static void
bind_created_at_property (GtkListItemFactory *factory,
                          GtkListItem        *list_item)
{
  GtkLabel *label = GTK_LABEL (gtk_list_item_get_child (list_item));
  FoundryForgeIssue *issue = FOUNDRY_FORGE_ISSUE (gtk_list_item_get_item (list_item));
  g_autoptr(GDateTime) created_at = NULL;
  g_autofree char *text = NULL;

  created_at = foundry_forge_issue_dup_created_at (issue);
  if (created_at)
    text = g_date_time_format (created_at, "%Y-%m-%d %H:%M:%S");

  gtk_label_set_text (label, text ? text : "");
}

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FoundryForgeManager) forge_manager = NULL;
  g_autoptr(FoundryForgeProject) project = NULL;
  g_autoptr(FoundryForgeListing) listing = NULL;
  g_autoptr(FoundryForge) forge = NULL;
  g_autoptr(GtkSingleSelection) model = NULL;
  g_autoptr(FoundryContext) context = NULL;
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
                         "default-width", 600,
                         "default-height", 400,
                         NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      NULL);
  gtk_window_set_child (window, GTK_WIDGET (box));

  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "vexpand", TRUE,
                           NULL);
  gtk_box_append (box, GTK_WIDGET (scroller));

  forge_manager = foundry_context_dup_forge_manager (context);
  dex_await (foundry_service_when_ready (FOUNDRY_SERVICE (forge_manager)), NULL);

  if (!(forge = foundry_forge_manager_dup_forge (forge_manager)))
    g_error ("Project not configured with forge");

  if (!(project = dex_await_object (foundry_forge_find_project (forge), &error)))
    g_error ("Failed to find project in forge: %s", error->message);

  if (!(listing = dex_await_object (foundry_forge_project_list_issues (project, NULL), &error)))
    g_error ("Failed to get issue listing: %s", error->message);

  foundry_forge_listing_set_auto_load (listing, TRUE);

  dex_await (foundry_forge_listing_load_page (listing, 0), NULL);

  g_print ("Initial number of items: %u\n",
           g_list_model_get_n_items (G_LIST_MODEL (listing)));

  model = gtk_single_selection_new (g_object_ref (G_LIST_MODEL (listing)));

  columnview = g_object_new (GTK_TYPE_COLUMN_VIEW,
                             "height-request", 200,
                             "model", model,
                             NULL);
  gtk_scrolled_window_set_child (scroller, GTK_WIDGET (columnview));

  // Create columns for FoundryForgeIssue properties
  {
    GtkColumnViewColumn *column;
    GtkListItemFactory *factory;

    // ID column
    factory = gtk_signal_list_item_factory_new ();
    g_object_set_data (G_OBJECT (factory), "property", (gpointer) "id");
    g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_string_property), NULL);
    column = gtk_column_view_column_new ("ID", factory);
    gtk_column_view_append_column (columnview, column);

    // State column
    factory = gtk_signal_list_item_factory_new ();
    g_object_set_data (G_OBJECT (factory), "property", (gpointer) "state");
    g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_string_property), NULL);
    column = gtk_column_view_column_new ("State", factory);
    gtk_column_view_append_column (columnview, column);

    // Title column
    factory = gtk_signal_list_item_factory_new ();
    g_object_set_data (G_OBJECT (factory), "property", (gpointer) "title");
    g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_string_property), NULL);
    column = gtk_column_view_column_new ("Title", factory);
    gtk_column_view_column_set_expand (column, TRUE);
    gtk_column_view_append_column (columnview, column);

    // Created At column
    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_created_at_property), NULL);
    column = gtk_column_view_column_new ("Created At", factory);
    gtk_column_view_append_column (columnview, column);
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
  if (argc != 2)
    {
      g_printerr ("usage: %s PROJECT_DIR\n", argv[0]);
      return 1;
    }

  dirpath = argv[1];

  gtk_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 8*1024*1024, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
