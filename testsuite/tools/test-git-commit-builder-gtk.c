/* test-git-commit-builder-gtk.c
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
#include <gtksourceview/gtksource.h>

static GMainLoop *main_loop;
static const char *project_dir;
static GFile *project_dir_file = NULL;
static FoundryGitCommitBuilder *commit_builder = NULL;
static GtkSourceView *diff_textview = NULL;
static GtkSourceBuffer *diff_buffer = NULL;
static GFile *current_file = NULL;
static gboolean current_file_is_staged = FALSE;
static GtkButton *stage_button = NULL;
static GListModel *staged_list = NULL;
static GListModel *unstaged_list = NULL;
static GListModel *untracked_list = NULL;

#define DEV_NULL "/dev/null"

typedef struct
{
  GFile *file;
  gboolean is_staged;
} UpdateDiffState;

static void
update_diff_state_free (UpdateDiffState *state)
{
  if (state == NULL)
    return;

  g_clear_object (&state->file);
  g_free (state);
}

/* In a real app you should probably use listview for
 * line display so that you can do way more interesting
 * things than show diff text.
 */
static char *
generate_diff_text (FoundryVcsDelta *delta)
{
  g_autoptr(GListStore) hunks = NULL;
  g_autofree char *old_path = NULL;
  g_autofree char *new_path = NULL;
  FoundryVcsDeltaStatus status;
  g_autoptr(GError) error = NULL;
  GString *diff_text = g_string_new (NULL);
  const char *endptr;
  gsize errpos;

  old_path = foundry_vcs_delta_dup_old_path (delta);
  new_path = foundry_vcs_delta_dup_new_path (delta);
  status = foundry_vcs_delta_get_status (delta);

  /* Print diff header */
  if (status == FOUNDRY_VCS_DELTA_STATUS_RENAMED)
    g_string_append_printf (diff_text, "diff --git a/%s b/%s\n",
                            old_path ? old_path : DEV_NULL,
                            new_path ? new_path : DEV_NULL);
  else if (status == FOUNDRY_VCS_DELTA_STATUS_DELETED)
    g_string_append_printf (diff_text, "diff --git a/%s b/%s\n",
                            old_path ? old_path : DEV_NULL, DEV_NULL);
  else if (status == FOUNDRY_VCS_DELTA_STATUS_ADDED)
    g_string_append_printf (diff_text, "diff --git a/%s b/%s\n",
                            DEV_NULL, new_path ? new_path : DEV_NULL);
  else
    g_string_append_printf (diff_text, "diff --git a/%s b/%s\n",
                            old_path ? old_path : DEV_NULL,
                            new_path ? new_path : DEV_NULL);

  if (old_path && new_path && g_strcmp0 (old_path, new_path) != 0)
    g_string_append_printf (diff_text, "rename from %s\nrename to %s\n",
                            old_path, new_path);

  /* Get hunks */
  if (!(hunks = dex_await_object (foundry_vcs_delta_list_hunks (delta), &error)))
    {
      g_string_append_printf (diff_text, "Error listing hunks: %s\n", error->message);
      return g_string_free (diff_text, FALSE);
    }

  /* Print each hunk */
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (hunks)); i++)
    {
      g_autoptr(FoundryVcsDiffHunk) hunk = NULL;
      g_autoptr(GListStore) lines = NULL;
      g_autofree char *header = NULL;

      hunk = g_list_model_get_item (G_LIST_MODEL (hunks), i);
      header = foundry_vcs_diff_hunk_dup_header (hunk);

      if (header)
        g_string_append (diff_text, header);

      if (!(lines = dex_await_object (foundry_vcs_diff_hunk_list_lines (hunk), &error)))
        {
          g_string_append_printf (diff_text, "Error listing lines: %s\n", error->message);
          continue;
        }

      for (guint j = 0; j < g_list_model_get_n_items (G_LIST_MODEL (lines)); j++)
        {
          g_autoptr(FoundryVcsDiffLine) line = NULL;
          FoundryVcsDiffLineOrigin origin;
          g_autofree char *text = NULL;
          gboolean has_newline;

          line = g_list_model_get_item (G_LIST_MODEL (lines), j);
          origin = foundry_vcs_diff_line_get_origin (line);
          text = foundry_vcs_diff_line_dup_text (line);
          has_newline = foundry_vcs_diff_line_get_has_newline (line);

          if (g_ascii_isprint (origin))
            g_string_append_c (diff_text, (char)origin);

          if (text)
            g_string_append (diff_text, text);

          if (!has_newline)
            g_string_append_c (diff_text, '\n');
        }
    }

  if (!g_utf8_validate (diff_text->str, diff_text->len, &endptr))
    {
      errpos = endptr - diff_text->str;
      g_warning ("Failed to create valid UTF-8 at position: %u",
                 (guint)errpos);
    }

  return g_string_free (diff_text, FALSE);
}

static DexFuture *
load_untracked_file_fiber (gpointer data)
{
  g_autoptr(GFile) file = data;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  const guint8 *data_ptr;
  gsize len;
  GtkSourceBuffer *buffer;
  GtkSourceLanguageManager *lang_manager;
  GtkSourceLanguage *language;
  g_autofree char *basename = NULL;

  if (file == NULL)
    {
      buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (diff_textview)));
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "", 0);
      gtk_source_buffer_set_language (buffer, NULL);
      return dex_future_new_true ();
    }

  if (!(bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &error)))
    {
      buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (diff_textview)));
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), error ? error->message : "Failed to load file", -1);
      gtk_source_buffer_set_language (buffer, NULL);
      return dex_future_new_true ();
    }

  data_ptr = g_bytes_get_data (bytes, &len);

  if (!g_utf8_validate_len ((const char *)data_ptr, len, NULL))
    {
      buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (diff_textview)));
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "File is not valid UTF-8", -1);
      gtk_source_buffer_set_language (buffer, NULL);
      return dex_future_new_true ();
    }

  buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (diff_textview)));
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), (const char *)data_ptr, len);

  lang_manager = gtk_source_language_manager_get_default ();
  basename = g_file_get_basename (file);
  language = gtk_source_language_manager_guess_language (lang_manager, basename, NULL);
  gtk_source_buffer_set_language (buffer, language);

  return dex_future_new_true ();
}

static DexFuture *
update_diff_view_fiber (gpointer data)
{
  UpdateDiffState *state = data;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(FoundryVcsDelta) delta = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *diff_text = NULL;
  GtkSourceBuffer *buffer;
  GtkSourceLanguageManager *lang_manager;
  GtkSourceLanguage *language;

  if (state->file == NULL)
    {
      buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (diff_textview)));
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "", 0);
      gtk_source_buffer_set_language (buffer, NULL);
      return dex_future_new_true ();
    }

  if (state->is_staged)
    future = foundry_git_commit_builder_load_staged_delta (commit_builder, state->file);
  else
    future = foundry_git_commit_builder_load_unstaged_delta (commit_builder, state->file);

  if (!(delta = dex_await_object (dex_ref (future), &error)))
    {
      buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (diff_textview)));
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), error ? error->message : "Failed to load delta", -1);
      gtk_source_buffer_set_language (buffer, NULL);
      return dex_future_new_true ();
    }

  diff_text = generate_diff_text (delta);
  buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (diff_textview)));
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), diff_text, strlen (diff_text));

  lang_manager = gtk_source_language_manager_get_default ();
  language = gtk_source_language_manager_get_language (lang_manager, "diff");
  gtk_source_buffer_set_language (buffer, language);

  return dex_future_new_true ();
}

static gboolean
is_file_in_list (GFile     *file,
                 GListModel *list)
{
  guint n_items = g_list_model_get_n_items (list);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GFile) item = g_list_model_get_item (G_LIST_MODEL (list), i);

      if (g_file_equal (file, item))
        return TRUE;
    }

  return FALSE;
}

static void
update_stage_button (void)
{
  gboolean is_staged;

  if (stage_button == NULL || current_file == NULL)
    {
      if (stage_button != NULL)
        gtk_widget_set_sensitive (GTK_WIDGET (stage_button), FALSE);
      return;
    }

  is_staged = is_file_in_list (current_file, staged_list);

  gtk_widget_set_sensitive (GTK_WIDGET (stage_button), TRUE);

  if (is_staged)
    gtk_button_set_label (stage_button, "Unstage File");
  else
    gtk_button_set_label (stage_button, "Stage File");
}

static void
update_diff_view (GFile    *file,
                  gboolean  is_staged)
{
  UpdateDiffState *state;

  g_clear_object (&current_file);
  current_file = file ? g_object_ref (file) : NULL;
  current_file_is_staged = is_staged;

  update_stage_button ();

  state = g_new0 (UpdateDiffState, 1);
  state->file = file ? g_object_ref (file) : NULL;
  state->is_staged = is_staged;

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          update_diff_view_fiber,
                                          state,
                                          (GDestroyNotify) update_diff_state_free));
}

static void
on_untracked_activate (GtkListView *listview,
                       guint         position,
                       gpointer      user_data)
{
  GtkSelectionModel *model;
  g_autoptr(GFile) file = NULL;

  model = gtk_list_view_get_model (listview);
  file = g_list_model_get_item (G_LIST_MODEL (model), position);

  g_clear_object (&current_file);
  current_file = file ? g_object_ref (file) : NULL;
  current_file_is_staged = FALSE;
  update_stage_button ();

  if (file != NULL)
    dex_future_disown (dex_scheduler_spawn (NULL, 0, load_untracked_file_fiber, g_steal_pointer (&file), NULL));
  else
    {
      GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (diff_textview)));
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "", 0);
      gtk_source_buffer_set_language (buffer, NULL);
    }
}

static void
on_unstaged_activate (GtkListView *listview,
                      guint         position,
                      gpointer      user_data)
{
  GtkSelectionModel *model;
  g_autoptr(GFile) file = NULL;

  model = gtk_list_view_get_model (listview);
  file = g_list_model_get_item (G_LIST_MODEL (model), position);

  update_diff_view (file, FALSE);
}

static void
on_staged_activate (GtkListView *listview,
                    guint         position,
                    gpointer      user_data)
{
  GtkSelectionModel *model;
  g_autoptr(GFile) file = NULL;

  model = gtk_list_view_get_model (listview);
  file = g_list_model_get_item (G_LIST_MODEL (model), position);

  update_diff_view (file, TRUE);
}

static void
setup_row (GtkSignalListItemFactory *factory,
          GtkListItem              *item,
          gpointer                  user_data)
{
  gtk_list_item_set_child (item,
                           g_object_new (GTK_TYPE_LABEL,
                                        "xalign", 0.f,
                                        NULL));
}

static void
bind_row (GtkSignalListItemFactory *factory,
         GtkListItem              *item,
         gpointer                  user_data)
{
  GFile *file = gtk_list_item_get_item (item);
  GtkLabel *label = GTK_LABEL (gtk_list_item_get_child (item));
  g_autofree char *relative_path = g_file_get_relative_path (project_dir_file, file);

  if (relative_path == NULL)
    relative_path = g_file_get_path (file);

  gtk_label_set_label (label, relative_path);
}

static void
update_style_scheme (GtkSettings *settings,
                     GParamSpec  *pspec,
                     gpointer     user_data)
{
  GtkSourceStyleSchemeManager *scheme_manager;
  GtkSourceStyleScheme *scheme;
  gboolean prefer_dark;

  if (diff_buffer == NULL)
    return;

  g_object_get (settings, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);

  scheme_manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (scheme_manager,
                                                       prefer_dark ? "Adwaita-dark" : "Adwaita");
  gtk_source_buffer_set_style_scheme (diff_buffer, scheme);
}

static void
on_list_items_changed (GListModel *model,
                       guint       position,
                       guint       removed,
                       guint       added,
                       gpointer    user_data)
{
  if (current_file != NULL)
    update_stage_button ();
}

static void
on_stage_button_clicked (GtkButton *button,
                         gpointer   user_data)
{
  gboolean is_staged;
  DexFuture *future;

  if (current_file == NULL || commit_builder == NULL)
    return;

  is_staged = is_file_in_list (current_file, staged_list);

  if (is_staged)
    future = foundry_git_commit_builder_unstage_file (commit_builder, current_file);
  else
    future = foundry_git_commit_builder_stage_file (commit_builder, current_file);

  dex_future_disown (future);
}

static DexFuture *
main_fiber (gpointer data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(FoundryVcsManager) vcs_manager = NULL;
  g_autoptr(FoundryVcs) vcs = NULL;
  g_autoptr(FoundryGitVcs) git_vcs = NULL;
  g_autoptr(GtkListItemFactory) factory = NULL;
  g_autoptr(GtkSelectionModel) untracked_model = NULL;
  g_autoptr(GtkSelectionModel) unstaged_model = NULL;
  g_autoptr(GtkSelectionModel) staged_model = NULL;
  g_autofree char *foundry_dir = NULL;
  GtkScrolledWindow *untracked_scroller;
  GtkScrolledWindow *unstaged_scroller;
  GtkScrolledWindow *staged_scroller;
  GtkListView *untracked_listview;
  GtkListView *unstaged_listview;
  GtkListView *staged_listview;
  GtkBox *box;
  GtkPaned *hpaned;
  GtkScrolledWindow *diff_scroller;
  GtkSourceView *textview;
  GtkSourceBuffer *text_buffer;
  GtkWindow *window;

  dex_await (foundry_init (), NULL);

  if (!(foundry_dir = dex_await_string (foundry_context_discover (project_dir, NULL), &error)))
    g_error ("%s", error->message);

  if (!(context = dex_await_object (foundry_context_new (foundry_dir, project_dir, FOUNDRY_CONTEXT_FLAGS_NONE, NULL), &error)))
    g_error ("%s", error->message);

  vcs_manager = foundry_context_dup_vcs_manager (context);
  vcs = foundry_vcs_manager_dup_vcs (vcs_manager);

  if (vcs == NULL || !FOUNDRY_IS_GIT_VCS (vcs))
    g_error ("No Git VCS found for project");

  git_vcs = FOUNDRY_GIT_VCS (g_object_ref (vcs));

  if (!(commit_builder = dex_await_object (foundry_git_commit_builder_new (git_vcs, NULL, 0), &error)))
    g_error ("%s", error->message);

  staged_list = g_object_ref (foundry_git_commit_builder_list_staged (commit_builder));
  unstaged_list = g_object_ref (foundry_git_commit_builder_list_unstaged (commit_builder));
  untracked_list = g_object_ref (foundry_git_commit_builder_list_untracked (commit_builder));

  project_dir_file = g_file_new_for_path (project_dir);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 800,
                         "default-height", 600,
                         NULL);

  hpaned = g_object_new (GTK_TYPE_PANED,
                         "orientation", GTK_ORIENTATION_HORIZONTAL,
                         "position", 300,
                         NULL);
  gtk_window_set_child (window, GTK_WIDGET (hpaned));

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      NULL);
  gtk_paned_set_start_child (hpaned, GTK_WIDGET (box));

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_row), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_row), NULL);

  gtk_box_append (box, g_object_new (GTK_TYPE_LABEL,
                                     "label", "Untracked",
                                     "xalign", 0.f,
                                     "margin-start", 6,
                                     "margin-top", 6,
                                     NULL));

  untracked_model = GTK_SELECTION_MODEL (gtk_no_selection_new (g_object_ref (untracked_list)));
  untracked_listview = g_object_new (GTK_TYPE_LIST_VIEW,
                                     "factory", factory,
                                     "model", untracked_model,
                                     "single-click-activate", TRUE,
                                     NULL);
  untracked_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                     "vexpand", TRUE,
                                     NULL);
  gtk_scrolled_window_set_child (untracked_scroller, GTK_WIDGET (untracked_listview));
  gtk_box_append (box, GTK_WIDGET (untracked_scroller));
  g_signal_connect (untracked_listview, "activate", G_CALLBACK (on_untracked_activate), NULL);

  gtk_box_append (box, g_object_new (GTK_TYPE_LABEL,
                                     "label", "Unstaged",
                                     "xalign", 0.f,
                                     "margin-start", 6,
                                     "margin-top", 6,
                                     NULL));

  unstaged_model = GTK_SELECTION_MODEL (gtk_no_selection_new (g_object_ref (unstaged_list)));
  unstaged_listview = g_object_new (GTK_TYPE_LIST_VIEW,
                                    "factory", factory,
                                    "model", unstaged_model,
                                    "single-click-activate", TRUE,
                                    NULL);
  unstaged_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                    "vexpand", TRUE,
                                    NULL);
  gtk_scrolled_window_set_child (unstaged_scroller, GTK_WIDGET (unstaged_listview));
  gtk_box_append (box, GTK_WIDGET (unstaged_scroller));
  g_signal_connect (unstaged_listview, "activate", G_CALLBACK (on_unstaged_activate), NULL);

  gtk_box_append (box, g_object_new (GTK_TYPE_LABEL,
                                     "label", "Staged",
                                     "xalign", 0.f,
                                     "margin-start", 6,
                                     "margin-top", 6,
                                     NULL));

  g_signal_connect (staged_list, "items-changed", G_CALLBACK (on_list_items_changed), NULL);
  g_signal_connect (unstaged_list, "items-changed", G_CALLBACK (on_list_items_changed), NULL);
  g_signal_connect (untracked_list, "items-changed", G_CALLBACK (on_list_items_changed), NULL);

  staged_model = GTK_SELECTION_MODEL (gtk_no_selection_new (g_object_ref (staged_list)));
  staged_listview = g_object_new (GTK_TYPE_LIST_VIEW,
                                  "factory", factory,
                                  "model", staged_model,
                                  "single-click-activate", TRUE,
                                  NULL);
  staged_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                  "vexpand", TRUE,
                                  NULL);
  gtk_scrolled_window_set_child (staged_scroller, GTK_WIDGET (staged_listview));
  gtk_box_append (box, GTK_WIDGET (staged_scroller));
  g_signal_connect (staged_listview, "activate", G_CALLBACK (on_staged_activate), NULL);

  text_buffer = gtk_source_buffer_new (NULL);
  diff_buffer = text_buffer;
  textview = GTK_SOURCE_VIEW (gtk_source_view_new_with_buffer (text_buffer));
  gtk_widget_set_hexpand (GTK_WIDGET (textview), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (textview), TRUE);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
  gtk_text_view_set_monospace (GTK_TEXT_VIEW (textview), TRUE);
  diff_textview = textview;

  {
    GtkSettings *settings;
    GtkSourceStyleSchemeManager *scheme_manager;
    GtkSourceStyleScheme *scheme;
    gboolean prefer_dark;

    settings = gtk_settings_get_default ();
    g_object_get (settings, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);

    scheme_manager = gtk_source_style_scheme_manager_get_default ();
    scheme = gtk_source_style_scheme_manager_get_scheme (scheme_manager,
                                                         prefer_dark ? "Adwaita-dark" : "Adwaita");
    gtk_source_buffer_set_style_scheme (text_buffer, scheme);

    g_signal_connect (settings,
                      "notify::gtk-application-prefer-dark-theme",
                      G_CALLBACK (update_style_scheme),
                      NULL);
  }

  diff_scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                "hexpand", TRUE,
                                "vexpand", TRUE,
                                NULL);
  gtk_scrolled_window_set_child (diff_scroller, GTK_WIDGET (textview));

  {
    GtkBox *diff_vbox;
    GtkButton *button;

    diff_vbox = g_object_new (GTK_TYPE_BOX,
                              "orientation", GTK_ORIENTATION_VERTICAL,
                              "hexpand", TRUE,
                              "vexpand", TRUE,
                              NULL);
    gtk_box_append (diff_vbox, GTK_WIDGET (diff_scroller));

    button = g_object_new (GTK_TYPE_BUTTON,
                           "label", "Stage File",
                           "halign", GTK_ALIGN_END,
                           "margin-start", 6,
                           "margin-end", 6,
                           "margin-top", 6,
                           "margin-bottom", 6,
                           "sensitive", FALSE,
                           NULL);
    stage_button = button;
    gtk_box_append (diff_vbox, GTK_WIDGET (button));

    g_signal_connect (button, "clicked", G_CALLBACK (on_stage_button_clicked), NULL);

    gtk_paned_set_end_child (hpaned, GTK_WIDGET (diff_vbox));
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

  project_dir = argv[1];

  gtk_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (NULL, 0, main_fiber, NULL, NULL));
  g_main_loop_run (main_loop);

  return 0;
}
