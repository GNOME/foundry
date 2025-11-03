/* foundry-file-row.c
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

#include "foundry-file-dialog-private.h"
#include "foundry-file-row.h"

/**
 * FoundryFileRow:
 *
 * An `AdwPreferencesRow` that allows typing or navigating to a
 * file or directory path. It is capable of expanding relative
 * paths from the user's home directory.
 *
 * Since: 1.1
 */

typedef struct
{
  GFileType file_type;
} FoundryFileRowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (FoundryFileRow, foundry_file_row, ADW_TYPE_ENTRY_ROW)

enum {
  PROP_0,
  PROP_FILE,
  PROP_FILE_TYPE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_file_row_selct_file_fiber (gpointer user_data)
{
  FoundryFileRow *self = user_data;
  g_autoptr(GtkFileDialog) dialog = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) selected = NULL;
  GFileType file_type;
  GtkRoot *root;

  g_assert (FOUNDRY_IS_FILE_ROW (self));

  file_type = foundry_file_row_get_file_type (self);

  if (!(root = gtk_widget_get_root (GTK_WIDGET (self))) || !GTK_IS_WINDOW (root))
    return foundry_future_new_disposed ();

  if (!(file = foundry_file_row_dup_file (self)))
    file = g_file_new_for_path (g_get_home_dir ());

  dialog = gtk_file_dialog_new ();

  if (file_type == G_FILE_TYPE_DIRECTORY)
    {
      gtk_file_dialog_set_initial_folder (dialog, file);

      if (!(selected = dex_await_object (foundry_file_dialog_select_folder (dialog, GTK_WINDOW (root)), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }
  else
    {
      gtk_file_dialog_set_initial_file (dialog, file);

      if (!(selected = dex_await_object (foundry_file_dialog_open (dialog, GTK_WINDOW (root)), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  g_assert (G_IS_FILE (selected));

  foundry_file_row_set_file (self, selected);

  return NULL;
}

static void
select_file_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameter)
{
  FoundryFileRow *self = (FoundryFileRow *)widget;

  g_assert (FOUNDRY_IS_FILE_ROW (self));

  dex_future_disown (dex_scheduler_spawn (NULL, 0,
                                          foundry_file_row_selct_file_fiber,
                                          g_object_ref (self),
                                          g_object_unref));

}

static void
foundry_file_row_notify_text_cb (FoundryFileRow *self,
                                 GtkEditable    *editable)
{
  g_assert (FOUNDRY_IS_FILE_ROW (self));
  g_assert (GTK_IS_EDITABLE (editable));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);
}

static void
foundry_file_row_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), FOUNDRY_TYPE_FILE_ROW);

  G_OBJECT_CLASS (foundry_file_row_parent_class)->dispose (object);
}

static void
foundry_file_row_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  FoundryFileRow *self = FOUNDRY_FILE_ROW (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_take_object (value, foundry_file_row_dup_file (self));
      break;

    case PROP_FILE_TYPE:
      g_value_set_enum (value, foundry_file_row_get_file_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_row_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  FoundryFileRow *self = FOUNDRY_FILE_ROW (object);

  switch (prop_id)
    {
    case PROP_FILE:
      foundry_file_row_set_file (self, g_value_get_object (value));
      break;

    case PROP_FILE_TYPE:
      foundry_file_row_set_file_type (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_file_row_class_init (FoundryFileRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = foundry_file_row_dispose;
  object_class->get_property = foundry_file_row_get_property;
  object_class->set_property = foundry_file_row_set_property;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_FILE_TYPE] =
    g_param_spec_enum ("file-type", NULL, NULL,
                       G_TYPE_FILE_TYPE,
                       G_FILE_TYPE_DIRECTORY,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/app/devsuite/foundry-adw/ui/foundry-file-row.ui");
  gtk_widget_class_bind_template_callback (widget_class, foundry_file_row_notify_text_cb);
  gtk_widget_class_install_action (widget_class, "select-file", NULL, select_file_action);
}

static void
foundry_file_row_init (FoundryFileRow *self)
{
  FoundryFileRowPrivate *priv = foundry_file_row_get_instance_private (self);

  priv->file_type = G_FILE_TYPE_DIRECTORY;

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
foundry_file_row_new (void)
{
  return g_object_new (FOUNDRY_TYPE_FILE_ROW, NULL);
}

/**
 * foundry_file_row_dup_file:
 * @self: a [class@Foundry.FileRow]
 *
 * Returns: (transfer full) (nullable):
 */
GFile *
foundry_file_row_dup_file (FoundryFileRow *self)
{
  g_autofree char *scheme = NULL;
  g_autofree char *expanded = NULL;
  const char *text;

  g_return_val_if_fail (FOUNDRY_IS_FILE_ROW (self), NULL);

  if (!(text = gtk_editable_get_text (GTK_EDITABLE (self))) || text[0] == 0)
    return NULL;

  if ((scheme = g_uri_parse_scheme (text)))
    return g_file_new_for_uri (text);

  if ((expanded = foundry_path_expand (text)))
    return g_file_new_for_path (expanded);

  return NULL;
}

void
foundry_file_row_set_file (FoundryFileRow *self,
                           GFile          *file)
{
  g_autoptr(GFile) old_file = NULL;

  g_return_if_fail (FOUNDRY_IS_FILE_ROW (self));
  g_return_if_fail (!file || G_IS_FILE (file));

  old_file = foundry_file_row_dup_file (self);

  if (file == NULL)
    {
      gtk_editable_set_text (GTK_EDITABLE (self), "");
    }
  else if (g_file_is_native (file))
    {
      g_autofree char *path = g_file_get_path (file);
      g_autofree char *collapsed = foundry_path_collapse (path);

      gtk_editable_set_text (GTK_EDITABLE (self), collapsed);
    }
  else
    {
      g_autofree char *uri = g_file_get_uri (file);

      gtk_editable_set_text (GTK_EDITABLE (self), uri);
    }

  if (old_file == file ||
      (old_file != NULL && file != NULL && g_file_equal (old_file, file)))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);
}

/**
 * foundry_file_row_get_file_type:
 * @self: a [class@Foundry.FileRow]
 *
 * Returns: the type of file that should be selected
 *
 * Since: 1.1
 */
GFileType
foundry_file_row_get_file_type (FoundryFileRow *self)
{
  FoundryFileRowPrivate *priv = foundry_file_row_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_FILE_ROW (self), 0);

  return priv->file_type;
}

/**
 * foundry_file_row_set_file_type:
 * @self: a [class@Foundry.FileRow]
 * @file_type: the type of file to select
 *
 * @file_type must be either `G_FILE_TYPE_REGULAR` or
 * `G_FILE_TYPE_DIRECTORY`.
 *
 * Changing this property while the user is selecting a file is
 * undefined behavior.
 *
 * Since: 1.1
 */
void
foundry_file_row_set_file_type (FoundryFileRow *self,
                                GFileType       file_type)
{
  FoundryFileRowPrivate *priv = foundry_file_row_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_FILE_ROW (self));
  g_return_if_fail (file_type == G_FILE_TYPE_REGULAR ||
                    file_type == G_FILE_TYPE_DIRECTORY);

  if (file_type != priv->file_type)
    {
      priv->file_type = file_type;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE_TYPE]);
    }
}
