#include <foundry.h>
#include <gtk/gtk.h>

static FoundryLlmConversation *conversation;
static GMainLoop *main_loop;
static const char *model_name;

static void
entry_activate (GtkEntry *entry)
{
  g_autofree char *text = NULL;

  g_assert (GTK_IS_ENTRY (entry));

  text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));

  if (text[0] == 0)
    return;

  gtk_editable_set_text (GTK_EDITABLE (entry), "");

  foundry_llm_conversation_send_message (conversation, "user", text);
}

static void
setup_row (GtkSignalListItemFactory *factory,
           GtkListItem              *item,
           gpointer                  user_data)
{
  GtkBox *box;
  GtkLabel *role;
  GtkLabel *content;

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      NULL);
  role = g_object_new (GTK_TYPE_LABEL,
                       "width-request", 75,
                       "xalign", 0.f,
                       "yalign", 0.f,
                       NULL);
  content = g_object_new (GTK_TYPE_LABEL,
                          "hexpand", TRUE,
                          "xalign", 0.f,
                          "yalign", 0.f,
                          "wrap", TRUE,
                          "wrap-mode", GTK_WRAP_CHAR,
                          NULL);
  gtk_box_append (box, GTK_WIDGET (role));
  gtk_box_append (box, GTK_WIDGET (content));

  gtk_list_item_set_child (item, GTK_WIDGET (box));
}

static void
bind_row (GtkSignalListItemFactory *factory,
          GtkListItem              *item,
          gpointer                  user_data)
{
  FoundryLlmMessage *message = gtk_list_item_get_item (item);
  g_autofree char *role = foundry_llm_message_dup_role (message);
  g_autofree char *content = foundry_llm_message_dup_content (message);
  GtkWidget *box = gtk_list_item_get_child (item);

  gtk_label_set_label (GTK_LABEL (gtk_widget_get_first_child (box)), role);
  g_object_bind_property (message, "content",
                          gtk_widget_get_last_child (box), "label",
                          G_BINDING_SYNC_CREATE);
}

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GtkListItemFactory) factory = NULL;
  g_autoptr(GtkSelectionModel) model = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryLlmManager) llm_manager = NULL;
  g_autoptr(FoundryLlmModel) llm = NULL;
  g_autoptr(GListModel) history = NULL;
  GtkScrolledWindow *scroller;
  g_autofree char *path = NULL;
  GtkListView *listview;
  const char *dirpath;
  GtkWindow *window;
  GtkEntry *entry;
  GtkBox *box;

  dex_await (foundry_init (), NULL);

  dirpath = ".";

  if (!(path = dex_await_string (foundry_context_discover (dirpath, NULL), &error)))
    g_error ("%s", error->message);

  if (!(context = dex_await_object (foundry_context_new (path, dirpath, FOUNDRY_CONTEXT_FLAGS_NONE, NULL), &error)))
    g_error ("%s", error->message);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 400,
                         "default-height", 600,
                         NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      NULL);
  gtk_window_set_child (window, GTK_WIDGET (box));

  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "vexpand", TRUE,
                           NULL);
  gtk_box_append (box, GTK_WIDGET (scroller));

  entry = g_object_new (GTK_TYPE_ENTRY,
                        "margin-top", 6,
                        "margin-start", 6,
                        "margin-end", 6,
                        "margin-bottom", 6,
                        NULL);
  g_signal_connect (entry, "activate", G_CALLBACK (entry_activate), NULL);
  gtk_box_append (box, GTK_WIDGET (entry));

  llm_manager = foundry_context_dup_llm_manager (context);

  if (!(llm = dex_await_object (foundry_llm_manager_find_model (llm_manager, model_name), &error)))
    g_error ("%s", error->message);

  if (!(conversation = dex_await_object (foundry_llm_model_chat (llm, "You are a grumpy open source maintainer. Do your worst."), &error)))
    g_error ("%s", error->message);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_row), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_row), NULL);

  history = foundry_llm_conversation_list_history (conversation);
  model = GTK_SELECTION_MODEL (gtk_no_selection_new (g_object_ref (history)));
  listview = g_object_new (GTK_TYPE_LIST_VIEW,
                           "height-request", 200,
                           "factory", factory,
                           "model", model,
                           NULL);
  gtk_scrolled_window_set_child (scroller, GTK_WIDGET (listview));

  g_signal_connect_swapped (window,
                            "close-request",
                            G_CALLBACK (g_main_loop_quit),
                            main_loop);
  gtk_window_present (window);
  gtk_widget_grab_focus (GTK_WIDGET (entry));

  return NULL;
}

int
main (int   argc,
      char *argv[])
{
  if (argc != 2)
    {
      g_printerr ("usage: %s MODEL_NAME\n", argv[0]);
      return 1;
    }

  model_name = argv[1];

  gtk_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 0, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
