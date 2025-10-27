/* foundry-source-buffer.c
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

#include "foundry-source-buffer-private.h"

struct _FoundrySourceBuffer
{
  GtkSourceBuffer      parent_instance;
  FoundryContext      *context;
  char                *override_syntax;
  GFile               *file;
  GBytes              *contents;
  guint64              change_count;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_LANGUAGE_ID,
  PROP_OVERRIDE_SYNTAX,
  N_PROPS
};

static void text_buffer_iface_init (FoundryTextBufferInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (FoundrySourceBuffer, foundry_source_buffer, GTK_SOURCE_TYPE_BUFFER,
                               G_IMPLEMENT_INTERFACE (FOUNDRY_TYPE_TEXT_BUFFER, text_buffer_iface_init))

static GParamSpec *properties[N_PROPS];

static void
foundry_source_buffer_changed (GtkTextBuffer *buffer)
{
  FoundrySourceBuffer *self = (FoundrySourceBuffer *)buffer;

  g_assert (FOUNDRY_IS_SOURCE_BUFFER (self));

  self->change_count++;

  g_clear_pointer (&self->contents, g_bytes_unref);

  GTK_TEXT_BUFFER_CLASS (foundry_source_buffer_parent_class)->changed (buffer);

  foundry_text_buffer_emit_changed (FOUNDRY_TEXT_BUFFER (self));
}

static void
foundry_source_buffer_notify_language_cb (FoundrySourceBuffer *self)
{
  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_SOURCE_BUFFER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LANGUAGE_ID]);
}

static void
foundry_source_buffer_dispose (GObject *object)
{
  FoundrySourceBuffer *self = (FoundrySourceBuffer *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->file);

  g_clear_pointer (&self->override_syntax, g_free);
  g_clear_pointer (&self->contents, g_bytes_unref);

  G_OBJECT_CLASS (foundry_source_buffer_parent_class)->dispose (object);
}

static void
foundry_source_buffer_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  FoundrySourceBuffer *self = FOUNDRY_SOURCE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_LANGUAGE_ID:
      g_value_take_string (value, foundry_text_buffer_dup_language_id (FOUNDRY_TEXT_BUFFER (self)));
      break;

    case PROP_OVERRIDE_SYNTAX:
      g_value_take_string (value, foundry_source_buffer_dup_override_syntax (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_buffer_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  FoundrySourceBuffer *self = FOUNDRY_SOURCE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    case PROP_OVERRIDE_SYNTAX:
      foundry_source_buffer_set_override_syntax (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_source_buffer_class_init (FoundrySourceBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkTextBufferClass *text_buffer_class = GTK_TEXT_BUFFER_CLASS (klass);

  object_class->dispose = foundry_source_buffer_dispose;
  object_class->get_property = foundry_source_buffer_get_property;
  object_class->set_property = foundry_source_buffer_set_property;

  text_buffer_class->changed = foundry_source_buffer_changed;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         FOUNDRY_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_LANGUAGE_ID] =
    g_param_spec_string ("language-id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  /**
   * FoundrySourceBuffer:override-syntax:
   *
   * The GtkSourceLanguage identifier of the syntax to use, ignoring any
   * guessed language.
   */
  properties[PROP_OVERRIDE_SYNTAX] =
    g_param_spec_string ("override-syntax", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_source_buffer_init (FoundrySourceBuffer *self)
{
  g_signal_connect (self,
                    "notify::language",
                    G_CALLBACK (foundry_source_buffer_notify_language_cb),
                    NULL);
}

FoundrySourceBuffer *
_foundry_source_buffer_new (FoundryContext *context,
                            GFile          *file)
{
  GtkSourceStyleSchemeManager *sm;
  GtkSourceStyleScheme *scheme;
  FoundrySourceBuffer *self;

  g_return_val_if_fail (FOUNDRY_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (!file || G_IS_FILE (file), NULL);

  sm = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (sm, "Adwaita");

  self = g_object_new (FOUNDRY_TYPE_SOURCE_BUFFER,
                       "style-scheme", scheme,
                       NULL);
  self->context = g_object_ref (context);
  g_set_object (&self->file, file);

  return self;
}

GFile *
_foundry_source_buffer_dup_file (FoundrySourceBuffer *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_BUFFER (self), NULL);

  return self->file ? g_object_ref (self->file) : NULL;
}

void
_foundry_source_buffer_set_file (FoundrySourceBuffer *self,
                                 GFile               *file)
{
  g_assert (FOUNDRY_IS_SOURCE_BUFFER (self));
  g_assert (G_IS_FILE (file));

  g_set_object (&self->file, file);
}

static GBytes *
foundry_source_buffer_dup_contents (FoundryTextBuffer *buffer)
{
  FoundrySourceBuffer *self = (FoundrySourceBuffer *)buffer;

  g_assert (FOUNDRY_IS_SOURCE_BUFFER (self));

  if (self->contents == NULL)
    {
      GtkTextIter begin, end;
      char *text;
      gsize len;

      gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self), &begin, &end);

      text = gtk_text_iter_get_slice (&begin, &end);
      len = strlen (text);

      if (gtk_source_buffer_get_implicit_trailing_newline (GTK_SOURCE_BUFFER (self)))
        {
          text = g_realloc (text, len + 2);
          text[len++] = '\n';
          text[len] = 0;
        }

      self->contents = g_bytes_new_take (text, len);
    }

  return g_bytes_ref (self->contents);
}

static gint64
foundry_source_buffer_get_change_count (FoundryTextBuffer *buffer)
{
  return FOUNDRY_SOURCE_BUFFER (buffer)->change_count;
}

static char *
foundry_source_buffer_dup_language_id (FoundryTextBuffer *buffer)
{
  GtkSourceLanguage *language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));

  if (language != NULL)
    return g_strdup (gtk_source_language_get_id (language));

  return NULL;
}

typedef union _FoundrySourceIter
{
  FoundryTextIter                unused;
  struct {
    FoundryTextBuffer           *buffer;
    const FoundryTextIterVTable *vtable;
    GtkTextIter                  iter;
  };
} FoundrySourceIter;

#define get_text_iter(iter) (&((FoundrySourceIter *)iter)->iter)

static gunichar
foundry_source_iter_get_char (const FoundryTextIter *iter)
{
  return gtk_text_iter_get_char (get_text_iter (iter));
}

static gsize
foundry_source_iter_get_line (const FoundryTextIter *iter)
{
  return gtk_text_iter_get_line (get_text_iter (iter));
}

static gsize
foundry_source_iter_get_line_offset (const FoundryTextIter *iter)
{
  return gtk_text_iter_get_line_offset (get_text_iter (iter));
}

static gsize
foundry_source_iter_get_offset (const FoundryTextIter *iter)
{
  return gtk_text_iter_get_offset (get_text_iter (iter));
}

static gboolean
foundry_source_iter_backward_char (FoundryTextIter *iter)
{
  return gtk_text_iter_backward_char (get_text_iter (iter));
}

static gboolean
foundry_source_iter_forward_char (FoundryTextIter *iter)
{
  return gtk_text_iter_forward_char (get_text_iter (iter));
}

static gboolean
foundry_source_iter_forward_line (FoundryTextIter *iter)
{
  return gtk_text_iter_forward_line (get_text_iter (iter));
}

static gboolean
foundry_source_iter_ends_line (const FoundryTextIter *iter)
{
  return gtk_text_iter_ends_line (get_text_iter (iter));
}

static gboolean
foundry_source_iter_is_end (const FoundryTextIter *iter)
{
  return gtk_text_iter_is_end (get_text_iter (iter));
}

static gboolean
foundry_source_iter_starts_line (const FoundryTextIter *iter)
{
  return gtk_text_iter_starts_line (get_text_iter (iter));
}

static gboolean
foundry_source_iter_is_start (const FoundryTextIter *iter)
{
  return gtk_text_iter_is_start (get_text_iter (iter));
}

static gboolean
foundry_source_iter_move_to_line_and_offset (FoundryTextIter *iter,
                                             gsize            line,
                                             gsize            line_offset)
{
  return gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (iter->buffer),
                                                  get_text_iter (iter),
                                                  line, line_offset);
}

static FoundryTextIterVTable iter_vtable = {
  .backward_char = foundry_source_iter_backward_char,
  .ends_line = foundry_source_iter_ends_line,
  .forward_char = foundry_source_iter_forward_char,
  .forward_line = foundry_source_iter_forward_line,
  .get_char = foundry_source_iter_get_char,
  .get_line = foundry_source_iter_get_line,
  .get_line_offset = foundry_source_iter_get_line_offset,
  .get_offset = foundry_source_iter_get_offset,
  .is_end = foundry_source_iter_is_end,
  .is_start = foundry_source_iter_is_start,
  .move_to_line_and_offset = foundry_source_iter_move_to_line_and_offset,
  .starts_line = foundry_source_iter_starts_line,
};

static void
foundry_source_buffer_iter_init (FoundryTextBuffer *buffer,
                                 FoundryTextIter   *iter)
{
  FoundrySourceIter *real = (FoundrySourceIter *)iter;

  memset (real, 0, sizeof *iter);

  real->buffer = buffer;
  real->vtable = &iter_vtable;

  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &real->iter);
}

static void
text_buffer_iface_init (FoundryTextBufferInterface *iface)
{
  iface->dup_contents = foundry_source_buffer_dup_contents;
  iface->get_change_count = foundry_source_buffer_get_change_count;
  iface->dup_language_id = foundry_source_buffer_dup_language_id;
  iface->iter_init = foundry_source_buffer_iter_init;
}

/**
 * foundry_source_buffer_dup_override_syntax:
 * @self: a [class@FoundryGtk.SourceBuffer]
 *
 * Gets the syntax to be used, overriding any language guessing.
 *
 * `NULL` indicates to use the default guessed syntax.
 *
 * Returns: (transfer full) (nullable):
 */
char *
foundry_source_buffer_dup_override_syntax (FoundrySourceBuffer *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_BUFFER (self), NULL);

  return g_strdup (self->override_syntax);
}

void
foundry_source_buffer_set_override_syntax (FoundrySourceBuffer *self,
                                           const char          *override_syntax)
{
  g_return_if_fail (FOUNDRY_IS_SOURCE_BUFFER (self));

  if (g_set_str (&self->override_syntax, override_syntax))
    {
      if (override_syntax != NULL)
        {
          GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();
          GtkSourceLanguage *l = gtk_source_language_manager_get_language (lm, override_syntax);

          gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (self), l);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OVERRIDE_SYNTAX]);
    }
}

/**
 * foundry_source_buffer_dup_context:
 * @self: a [class@FoundryGtk.SourceBuffer]
 *
 * Returns: (transfer full): a [class@Foundry.Context]
 */
FoundryContext *
foundry_source_buffer_dup_context (FoundrySourceBuffer *self)
{
  g_return_val_if_fail (FOUNDRY_IS_SOURCE_BUFFER (self), NULL);

  return g_object_ref (self->context);
}

void
_foundry_source_buffer_init_iter (FoundrySourceBuffer *self,
                                  FoundryTextIter     *iter,
                                  const GtkTextIter   *where)
{
  FoundrySourceIter *real = (FoundrySourceIter *)iter;

  g_return_if_fail (FOUNDRY_IS_SOURCE_BUFFER (self));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (where != NULL);

  memset (real, 0, sizeof *iter);

  real->buffer = FOUNDRY_TEXT_BUFFER (self);
  real->vtable = &iter_vtable;
  real->iter = *where;
}

static gint
get_buffer_range_min_indent (GtkTextBuffer *buffer,
                             gint           start_line,
                             gint           end_line)
{
  GtkTextIter iter;
  gint current_indent;
  gint min_indent = G_MAXINT;

  for (gint line = start_line; line <= end_line; ++line)
    {
      current_indent = 0;
      gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
      while (!gtk_text_iter_ends_line (&iter) && g_unichar_isspace (gtk_text_iter_get_char (&iter)))
        {
          gtk_text_iter_forward_char (&iter);
          ++current_indent;
        }

      if (gtk_text_iter_ends_line (&iter))
        continue;
      else
        min_indent = MIN (min_indent, current_indent);
    }

  return min_indent;
}

static gboolean
is_line_commentable (GtkTextBuffer *buffer,
                     gint           line,
                     const char    *start_tag)
{
  g_autofree char *text = NULL;
  GtkTextIter iter;
  GtkTextIter end;

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
  if (gtk_text_iter_is_end (&iter))
    return FALSE;

  while (g_unichar_isspace (gtk_text_iter_get_char (&iter)))
    {
      if (gtk_text_iter_ends_line (&iter) ||
          !gtk_text_iter_forward_char (&iter))
        return FALSE;
    }

  end = iter;
  if (!gtk_text_iter_ends_line (&end))
    gtk_text_iter_forward_to_line_end (&end);

  text = gtk_text_iter_get_text (&iter, &end);
  if (g_str_has_prefix (text, start_tag))
    return FALSE;

  return TRUE;
}

static gboolean
is_line_uncommentable (GtkTextBuffer *buffer,
                       gint           line,
                       const char    *start_tag,
                       GtkTextIter   *start_tag_begin,
                       GtkTextIter   *start_tag_end)
{
  g_autofree char *text = NULL;
  GtkTextIter iter, end;

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
  if (gtk_text_iter_is_end (&iter))
    return FALSE;

  while (g_unichar_isspace (gtk_text_iter_get_char (&iter)))
    {
      if (gtk_text_iter_ends_line (&iter) ||
          !gtk_text_iter_forward_char (&iter))
        return FALSE;
    }

  end = iter;
  if (!gtk_text_iter_ends_line (&end))
    gtk_text_iter_forward_to_line_end (&end);

  text = gtk_text_iter_get_text (&iter, &end);
  if (g_str_has_prefix (text, start_tag))
    {
      *start_tag_begin = *start_tag_end = iter;
      gtk_text_iter_forward_chars (start_tag_end, g_utf8_strlen (start_tag, -1));
      return TRUE;
    }

  return FALSE;
}

static void
comment_line (GtkTextBuffer *buffer,
              const char    *start_tag,
              const char    *end_tag,
              gint           line,
              gint           start_offset,
              gboolean       is_block_tag)
{
  g_autofree char *start_tag_str = NULL;
  g_autofree char *end_tag_str = NULL;
  GtkTextIter start;
  GtkTextIter end_of_line;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (start_tag != NULL);
  g_assert ((is_block_tag && end_tag != NULL) || !is_block_tag);
  g_assert (line >= 0 && line < gtk_text_buffer_get_line_count (buffer));

  if (!is_line_commentable (buffer, line, start_tag))
    return;

  gtk_text_buffer_get_iter_at_line_offset (buffer, &start, line, start_offset);
  if (gtk_text_iter_ends_line (&start))
    return;

  start_tag_str = g_strconcat (start_tag, " ", NULL);
  gtk_text_buffer_insert (buffer, &start, start_tag_str, -1);

  if (!is_block_tag)
    return;

  end_of_line = start;
  gtk_text_iter_forward_to_line_end (&end_of_line);

  end_tag_str = g_strconcat (" ", end_tag, NULL);
  gtk_text_buffer_insert (buffer, &end_of_line, end_tag_str, -1);
}

static void
uncomment_line (GtkTextBuffer *buffer,
                const char    *start_tag,
                const char    *end_tag,
                gint           line,
                gboolean       is_block_tag)
{
  GtkTextIter tag_begin;
  GtkTextIter tag_end;
  GtkTextIter tmp_iter;
  GtkTextIter end_of_line;
  gunichar ch;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (start_tag != NULL);
  g_assert ((is_block_tag && end_tag != NULL) || !is_block_tag);
  g_assert (line >= 0 && line < gtk_text_buffer_get_line_count (buffer));

  if (!is_line_uncommentable (buffer, line, start_tag, &tag_begin, &tag_end))
    return;

  gtk_text_buffer_delete (buffer, &tag_begin, &tag_end);
  ch = gtk_text_iter_get_char (&tag_begin);
  if (ch == ' ' || ch == '\t')
    {
      gtk_text_iter_forward_char (&tag_end);
      gtk_text_buffer_delete (buffer, &tag_begin, &tag_end);
    }

  if (!is_block_tag)
    return;

  end_of_line = tag_begin;
  gtk_text_iter_forward_to_line_end (&end_of_line);

  if (gtk_text_iter_forward_search (&tag_begin, end_tag, GTK_TEXT_SEARCH_TEXT_ONLY, &tag_begin, &tag_end, NULL))
    {
      tmp_iter = tag_begin;
      gtk_text_iter_backward_char (&tmp_iter);
      ch = gtk_text_iter_get_char (&tmp_iter);
      if (ch == ' ' || ch == '\t')
        tag_begin = tmp_iter;

      tmp_iter = tag_end;
      if (!gtk_text_iter_ends_line (&tmp_iter))
        {
          gtk_text_iter_forward_char (&tmp_iter);
          ch = gtk_text_iter_get_char (&tmp_iter);
          if (ch == ' ' || ch == '\t')
            {
              tag_end = tmp_iter;
              gtk_text_iter_forward_char (&tag_end);
            }
        }

      gtk_text_buffer_delete (buffer, &tag_begin, &tag_end);
    }
}

/**
 * foundry_source_buffer_comment:
 * @self: a [class@FoundryGtk.SourceBuffer]
 * @begin: start of the text range to comment
 * @end: end of the text range to comment
 *
 * Comments the specified text range using the appropriate comment syntax
 * for the current language.
 *
 * Since: 1.1
 */
void
foundry_source_buffer_comment (FoundrySourceBuffer *self,
                               const GtkTextIter   *begin,
                               const GtkTextIter   *end)
{
  GtkSourceLanguage *lang;
  const char *start_tag;
  const char *end_tag = NULL;
  gint start_line;
  gint end_line;
  gint indent;
  gboolean block_comment = TRUE;
  GtkTextIter ordered_begin, ordered_end;

  g_return_if_fail (FOUNDRY_IS_SOURCE_BUFFER (self));
  g_return_if_fail (begin != NULL);
  g_return_if_fail (end != NULL);

  ordered_begin = *begin;
  ordered_end = *end;
  gtk_text_iter_order (&ordered_begin, &ordered_end);

  lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self));
  if (lang == NULL)
    return;

  if (g_strcmp0 (gtk_source_language_get_id (lang), "c") == 0)
    {
      if (!(start_tag = gtk_source_language_get_metadata (lang, "block-comment-start")) ||
          !(end_tag = gtk_source_language_get_metadata (lang, "block-comment-end")))
        {
          block_comment = FALSE;

          if (!(start_tag = gtk_source_language_get_metadata (lang, "line-comment-start")))
            return;
        }
    }
  else
    {
      if (!(start_tag = gtk_source_language_get_metadata (lang, "line-comment-start")))
        {
          if (!(start_tag = gtk_source_language_get_metadata (lang, "block-comment-start")) ||
              !(end_tag = gtk_source_language_get_metadata (lang, "block-comment-end")))
            return;
        }
      else
        block_comment = FALSE;
    }

  start_line = gtk_text_iter_get_line (&ordered_begin);
  end_line = gtk_text_iter_get_line (&ordered_end);

  if (gtk_text_iter_starts_line (&ordered_end))
    end_line--;

  indent = get_buffer_range_min_indent (GTK_TEXT_BUFFER (self), start_line, end_line);
  if (indent == G_MAXINT)
    return;

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (self));

  for (gint line = start_line; line <= end_line; ++line)
    comment_line (GTK_TEXT_BUFFER (self), start_tag, end_tag, line, indent, block_comment);

  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (self));
}

/**
 * foundry_source_buffer_uncomment:
 * @self: a [class@FoundryGtk.SourceBuffer]
 * @begin: start of the text range to uncomment
 * @end: end of the text range to uncomment
 *
 * Removes comment markers from the specified text range.
 *
 * Since: 1.1
 */
void
foundry_source_buffer_uncomment (FoundrySourceBuffer *self,
                                 const GtkTextIter   *begin,
                                 const GtkTextIter   *end)
{
  GtkSourceLanguage *lang;
  const char *start_tag;
  const char *end_tag = NULL;
  gint start_line;
  gint end_line;
  gboolean block_comment = TRUE;
  GtkTextIter ordered_begin, ordered_end;

  g_return_if_fail (FOUNDRY_IS_SOURCE_BUFFER (self));
  g_return_if_fail (begin != NULL);
  g_return_if_fail (end != NULL);

  ordered_begin = *begin;
  ordered_end = *end;
  gtk_text_iter_order (&ordered_begin, &ordered_end);

  if (!(lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self))))
    return;

  if (g_strcmp0 (gtk_source_language_get_id (lang), "c") == 0)
    {
      if (!(start_tag = gtk_source_language_get_metadata (lang, "block-comment-start")) ||
          !(end_tag = gtk_source_language_get_metadata (lang, "block-comment-end")))
        {
          block_comment = FALSE;

          if (!(start_tag = gtk_source_language_get_metadata (lang, "line-comment-start")))
            return;
        }
    }
  else
    {
      if (!(start_tag = gtk_source_language_get_metadata (lang, "line-comment-start")))
        {
          if (!(start_tag = gtk_source_language_get_metadata (lang, "block-comment-start")) ||
              !(end_tag = gtk_source_language_get_metadata (lang, "block-comment-end")))
            return;
        }
      else
        block_comment = FALSE;
    }

  start_line = gtk_text_iter_get_line (&ordered_begin);
  end_line = gtk_text_iter_get_line (&ordered_end);

  if (gtk_text_iter_starts_line (&ordered_end))
    end_line--;

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (self));

  for (gint line = start_line; line <= end_line; ++line)
    uncomment_line (GTK_TEXT_BUFFER (self), start_tag, end_tag, line, block_comment);

  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (self));
}
