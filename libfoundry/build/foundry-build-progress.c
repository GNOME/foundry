/* foundry-build-progress.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <glib/gstdio.h>

#include "line-reader-private.h"

#include "foundry-build-pipeline-private.h"
#include "foundry-build-progress-private.h"
#include "foundry-build-stage-private.h"
#include "foundry-directory-reaper.h"
#include "foundry-inhibitor.h"
#include "foundry-process-launcher.h"
#include "foundry-path.h"
#include "foundry-util.h"

#include "pty-intercept.h"

struct _FoundryBuildProgress
{
  FoundryContextual           parent_instance;
  FoundryBuildPipelinePhase   phase;
  GWeakRef                    pipeline;
  char                       *builddir;
  FoundryBuildStage          *current_stage;
  DexCancellable             *cancellable;
  GPtrArray                  *stages;
  GPtrArray                  *artifacts;
  DexFuture                  *fiber;
  char                       *errfmt_current_dir;
  char                       *errfmt_top_dir;
  GRegex                    **regexes;
  guint                       n_regexes;
  int                         pty_fd;
};

enum {
  PROP_0,
  PROP_BUILDDIR,
  PROP_PHASE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryBuildProgress, foundry_build_progress, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static gboolean
extract_directory_change (FoundryBuildProgress *self,
                          const guint8         *data,
                          gsize                 len)
{
  g_autofree gchar *dir = NULL;
  const guint8 *begin;

  g_assert (FOUNDRY_IS_BUILD_PIPELINE (self));

  if (len == 0)
    return FALSE;

#define ENTERING_DIRECTORY_BEGIN "Entering directory '"
#define ENTERING_DIRECTORY_END   "'"

  begin = memmem (data, len, ENTERING_DIRECTORY_BEGIN, strlen (ENTERING_DIRECTORY_BEGIN));
  if (begin == NULL)
    return FALSE;

  begin += strlen (ENTERING_DIRECTORY_BEGIN);

  if (data[len - 1] != '\'')
    return FALSE;

  len = &data[len - 1] - begin;
  dir = g_strndup ((gchar *)begin, len);

  if (g_utf8_validate (dir, len, NULL))
    {
      g_free (self->errfmt_current_dir);

      if (len == 0)
        self->errfmt_current_dir = g_strdup (self->errfmt_top_dir);
      else
        self->errfmt_current_dir = g_strndup (dir, len);

      if (self->errfmt_top_dir == NULL)
        self->errfmt_top_dir = g_strdup (self->errfmt_current_dir);

      return TRUE;
    }

#undef ENTERING_DIRECTORY_BEGIN
#undef ENTERING_DIRECTORY_END

  return FALSE;
}

static guint8 *
filter_color_codes (const guint8 *data,
                    gsize         len,
                    gsize        *out_len)
{
  g_autoptr(GByteArray) dst = NULL;

  g_return_val_if_fail (out_len != NULL, NULL);

  *out_len = 0;

  if (data == NULL)
    return NULL;
  else if (len == 0)
    return (guint8 *)g_strdup ("");

  dst = g_byte_array_sized_new (len);

  for (gsize i = 0; i < len; i++)
    {
      guint8 ch = data[i];
      guint8 next = (i+1) < len ? data[i+1] : 0;

      if (ch == '\\' && next == 'e')
        {
          i += 2;
        }
      else if (ch == '\033')
        {
          i++;
        }
      else
        {
          g_byte_array_append (dst, &ch, 1);
          continue;
        }

      if (i >= len)
        break;

      if (data[i] == '[')
        i++;

      if (i >= len)
        break;

      for (; i < len; i++)
        {
          ch = data[i];

          if (g_ascii_isdigit (ch) || ch == ' ' || ch == ';')
            continue;

          break;
        }
    }

  *out_len = dst->len;

  return g_byte_array_free (g_steal_pointer (&dst), FALSE);
}

static void
extract_diagnostics (FoundryBuildProgress *self,
                     const guint8         *data,
                     gsize                 len)
{
  g_autofree guint8 *unescaped = NULL;
  LineReader reader;
  gsize line_len;
  char *line;

  g_assert (FOUNDRY_IS_BUILD_PIPELINE (self));
  g_assert (data != NULL);

  if (len == 0 || self->n_regexes == 0)
    return;

  /* If we have any color escape sequences, remove them */
  if G_UNLIKELY (memchr (data, '\033', len) || memmem (data, len, "\\e", 2))
    {
      gsize out_len = 0;

      unescaped = filter_color_codes (data, len, &out_len);

      if (out_len == 0)
        return;

      data = unescaped;
      len = out_len;
    }

  line_reader_init (&reader, (char *)data, len);

  while (NULL != (line = line_reader_next (&reader, &line_len)))
    {
      if (extract_directory_change (self, (const guint8 *)line, line_len))
        continue;

      for (guint i = 0; i < self->n_regexes; i++)
        {
          const GRegex *regex = self->regexes[i];
          g_autoptr(GMatchInfo) match_info = NULL;

          if (g_regex_match_full (regex, line, line_len, 0, 0, &match_info, NULL))
            {
#if 0
              g_autoptr(IdeDiagnostic) diagnostic = create_diagnostic (self, match_info);

              if (diagnostic != NULL)
                {
                  ide_pipeline_emit_diagnostic (self, diagnostic);
                  break;
                }
#endif
            }
        }
    }
}

static void
intercept_pty_consumer_cb (const PtyIntercept     *intercept,
                           const PtyInterceptSide *side,
                           const guint8           *data,
                           gsize                   len,
                           gpointer                user_data)
{
  FoundryBuildProgress *self = user_data;

  g_assert (intercept != NULL);
  g_assert (side != NULL);
  g_assert (data != NULL);
  g_assert (len > 0);
  g_assert (FOUNDRY_IS_BUILD_PROGRESS (self));

  extract_diagnostics (self, data, len);
}

static void
foundry_build_progress_dispose (GObject *object)
{
  FoundryBuildProgress *self = (FoundryBuildProgress *)object;

  g_clear_pointer (&self->errfmt_current_dir, g_free);
  g_clear_pointer (&self->errfmt_top_dir, g_free);

  g_clear_object (&self->current_stage);

  g_clear_fd (&self->pty_fd, NULL);

  if (self->stages->len > 0)
    g_ptr_array_remove_range (self->stages, 0, self->stages->len);

  g_weak_ref_set (&self->pipeline, NULL);

  G_OBJECT_CLASS (foundry_build_progress_parent_class)->dispose (object);
}

static void
foundry_build_progress_finalize (GObject *object)
{
  FoundryBuildProgress *self = (FoundryBuildProgress *)object;

  for (guint i = 0; i < self->n_regexes; i++)
    g_regex_unref (self->regexes[i]);

  g_clear_pointer (&self->regexes, g_free);
  g_clear_pointer (&self->artifacts, g_ptr_array_unref);
  g_clear_pointer (&self->stages, g_ptr_array_unref);

  dex_clear (&self->cancellable);
  dex_clear (&self->fiber);

  g_clear_pointer (&self->builddir, g_free);

  g_weak_ref_clear (&self->pipeline);

  G_OBJECT_CLASS (foundry_build_progress_parent_class)->finalize (object);
}

static void
foundry_build_progress_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  FoundryBuildProgress *self = FOUNDRY_BUILD_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_BUILDDIR:
      g_value_set_string (value, self->builddir);
      break;

    case PROP_PHASE:
      g_value_set_flags (value, foundry_build_progress_get_phase (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_build_progress_class_init (FoundryBuildProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = foundry_build_progress_dispose;
  object_class->finalize = foundry_build_progress_finalize;
  object_class->get_property = foundry_build_progress_get_property;

  properties [PROP_BUILDDIR] =
    g_param_spec_string ("builddir", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PHASE] =
    g_param_spec_flags ("phase", NULL, NULL,
                        FOUNDRY_TYPE_BUILD_PIPELINE_PHASE,
                        FOUNDRY_BUILD_PIPELINE_PHASE_NONE,
                        (G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_build_progress_init (FoundryBuildProgress *self)
{
  self->pty_fd = -1;
  self->stages = g_ptr_array_new_with_free_func (g_object_unref);
  self->artifacts = g_ptr_array_new_with_free_func (g_object_unref);
  g_weak_ref_init (&self->pipeline, NULL);
}

/**
 * foundry_build_progress_await:
 * @self: a #FoundryBuildProgress
 *
 * Gets a [class@Dex.Future] that will resolve when the progress
 * has completed running.
 *
 * Returns: (transfer full): a [class@Dex.Future].
 */
DexFuture *
foundry_build_progress_await (FoundryBuildProgress *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));

  if (self->fiber == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_INITIALIZED,
                                  "Attempt to await build progress without an operation");

  return dex_ref (self->fiber);
}

FoundryBuildProgress *
_foundry_build_progress_new (FoundryBuildPipeline      *pipeline,
                             DexCancellable            *cancellable,
                             FoundryBuildPipelinePhase  phase,
                             GPtrArray                 *regexes,
                             int                        pty_fd)
{
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GError) error = NULL;
  FoundryBuildProgress *self;
  GListModel *model;
  guint n_stages;

  g_return_val_if_fail (FOUNDRY_IS_BUILD_PIPELINE (pipeline), NULL);
  g_return_val_if_fail (FOUNDRY_BUILD_PIPELINE_PHASE_MASK (phase) != 0, NULL);
  g_return_val_if_fail (DEX_IS_CANCELLABLE (cancellable), NULL);

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (pipeline));

  model = G_LIST_MODEL (pipeline);
  n_stages = g_list_model_get_n_items (model);

  self = g_object_new (FOUNDRY_TYPE_BUILD_PROGRESS,
                       "context", context,
                       NULL);

  if (regexes != NULL && regexes->len > 0)
    {
      self->regexes = g_new0 (GRegex *, regexes->len);
      self->n_regexes = regexes->len;

      for (guint i = 0; i < regexes->len; i++)
        self->regexes[i] = g_regex_ref (g_ptr_array_index (regexes, i));
    }

  self->phase = phase;
  self->pty_fd = dup (pty_fd);
  self->cancellable = dex_ref (cancellable);
  self->builddir = foundry_build_pipeline_dup_builddir (pipeline);
  g_weak_ref_set (&self->pipeline, pipeline);

  for (guint i = 0; i < n_stages; i++)
    {
      g_autoptr(FoundryBuildStage) stage = g_list_model_get_item (model, i);

      if (_foundry_build_stage_matches (stage, phase))
        g_ptr_array_add (self->stages, g_steal_pointer (&stage));
    }

  return self;
}

static DexFuture *
foundry_build_progress_build_fiber (gpointer user_data)
{
  FoundryBuildProgress *self = user_data;
  g_autoptr(FoundryInhibitor) inhibitor = NULL;
  g_autoptr(FoundryBuildPipeline) pipeline = NULL;
  g_autofree char *builddir = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_BUILD_PROGRESS (self));
  g_assert (self->builddir != NULL);

  if (!(inhibitor = foundry_contextual_inhibit (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(pipeline = g_weak_ref_get (&self->pipeline)))
    return foundry_future_new_disposed ();

  if (!dex_await (foundry_mkdir_with_parents (self->builddir, 0750), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < self->stages->len; i++)
    {
      FoundryBuildStage *stage = g_ptr_array_index (self->stages, i);
      FoundryBuildPipelinePhase phase = foundry_build_stage_get_phase (stage);

      if (g_set_object (&self->current_stage, stage))
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PHASE]);

      if (!dex_await (foundry_build_stage_query (stage), &error))
        {
          g_warning ("%s query failed: %s", G_OBJECT_TYPE_NAME (stage), error->message);
          g_clear_error (&error);
        }

      if (foundry_build_stage_get_completed (stage))
        continue;

      if (!dex_await (foundry_build_stage_build (stage, self), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      /* Reset compile commands if this might have affected it */
      if (phase == FOUNDRY_BUILD_PIPELINE_PHASE_CONFIGURE)
        _foundry_build_pipeline_reset_compile_commands (pipeline);
    }

  if (g_set_object (&self->current_stage, NULL))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PHASE]);

  return dex_future_new_true ();
}

DexFuture *
_foundry_build_progress_build (FoundryBuildProgress *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));
  dex_return_error_if_fail (self->fiber == NULL);

  self->fiber = dex_scheduler_spawn (NULL, 0,
                                     foundry_build_progress_build_fiber,
                                     g_object_ref (self),
                                     g_object_unref);

  return foundry_build_progress_await (self);
}

static DexFuture *
foundry_build_progress_clean_fiber (gpointer user_data)
{
  FoundryBuildProgress *self = user_data;
  g_autoptr(FoundryInhibitor) inhibitor = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (FOUNDRY_IS_BUILD_PROGRESS (self));

  if (!(inhibitor = foundry_contextual_inhibit (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = self->stages->len; i > 0; i--)
    {
      g_autoptr(FoundryBuildStage) stage = g_object_ref (g_ptr_array_index (self->stages, i - 1));

      g_assert (FOUNDRY_IS_BUILD_STAGE (stage));

      if (!dex_await (foundry_build_stage_clean (stage, self), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

DexFuture *
_foundry_build_progress_clean (FoundryBuildProgress *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));
  dex_return_error_if_fail (self->fiber == NULL);

  self->fiber = dex_scheduler_spawn (NULL, 0,
                                     foundry_build_progress_clean_fiber,
                                     g_object_ref (self),
                                     g_object_unref);

  return foundry_build_progress_await (self);
}

static DexFuture *
foundry_build_progress_purge_fiber (gpointer user_data)
{
  FoundryBuildProgress *self = user_data;
  g_autoptr(FoundryDirectoryReaper) reaper = NULL;
  g_autoptr(FoundryInhibitor) inhibitor = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) builddir = NULL;

  g_assert (FOUNDRY_IS_BUILD_PROGRESS (self));

  if (!(inhibitor = foundry_contextual_inhibit (FOUNDRY_CONTEXTUAL (self), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = self->stages->len; i > 0; i--)
    {
      g_autoptr(FoundryBuildStage) stage = g_object_ref (g_ptr_array_index (self->stages, i - 1));

      if (!dex_await (foundry_build_stage_purge (stage, self), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  builddir = g_file_new_for_path (self->builddir);
  reaper = foundry_directory_reaper_new ();

  foundry_directory_reaper_add_directory (reaper, builddir, 0);
  foundry_directory_reaper_add_file (reaper, builddir, 0);

  if (!dex_await (foundry_directory_reaper_execute (reaper), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

DexFuture *
_foundry_build_progress_purge (FoundryBuildProgress *self)
{
  dex_return_error_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));
  dex_return_error_if_fail (self->fiber == NULL);

  self->fiber = dex_scheduler_spawn (NULL, 0,
                                     foundry_build_progress_purge_fiber,
                                     g_object_ref (self),
                                     g_object_unref);

  return foundry_build_progress_await (self);
}

/**
 * foundry_build_progress_print: (skip)
 * @self: a [class@Foundry.BuildProgress]
 *
 * Prints a message to the build pipeline PTY device.
 */
void
foundry_build_progress_print (FoundryBuildProgress *self,
                              const char           *format,
                              ...)
{
  g_autofree char *message = NULL;
  va_list args;

  g_return_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));

  if (self->pty_fd < 0)
    return;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  write (self->pty_fd, message, strlen (message));
}

void
foundry_build_progress_setup_pty (FoundryBuildProgress   *self,
                                  FoundryProcessLauncher *launcher)
{
  g_return_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));
  g_return_if_fail (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  if (self->pty_fd == -1)
    return;

  foundry_process_launcher_take_fd (launcher, dup (self->pty_fd), STDIN_FILENO);
  foundry_process_launcher_take_fd (launcher, dup (self->pty_fd), STDOUT_FILENO);
  foundry_process_launcher_take_fd (launcher, dup (self->pty_fd), STDERR_FILENO);
}

/**
 * foundry_build_progress_dup_cancellable:
 * @self: a [class@Foundry.BuildProgress]
 *
 * Gets a cancellable that will reject when the build has been cancelled.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when the
 *   build operation has been cancelled.
 */
DexCancellable *
foundry_build_progress_dup_cancellable (FoundryBuildProgress *self)
{
  g_return_val_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self), NULL);

  return dex_ref (self->cancellable);
}

FoundryBuildPipelinePhase
foundry_build_progress_get_phase (FoundryBuildProgress *self)
{
  g_return_val_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self), 0);

  if (self->current_stage != NULL)
    return foundry_build_stage_get_phase (self->current_stage);

  return FOUNDRY_BUILD_PIPELINE_PHASE_NONE;
}

void
foundry_build_progress_add_artifact (FoundryBuildProgress *self,
                                     GFile                *file)
{
  g_return_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self));
  g_return_if_fail (G_IS_FILE (file));

  g_ptr_array_add (self->artifacts, g_object_ref (file));
}

/**
 * foundry_build_progress_list_artifacts:
 * @self: a #FoundryBuildProgress
 *
 * Gets a #GListModel of #GFile representing build arifacts.
 *
 * This may include, for example, a path to a ".flatpak" bundle.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of [iface@Gio.File]
 */
GListModel *
foundry_build_progress_list_artifacts (FoundryBuildProgress *self)
{
  g_autoptr(GListStore) store = NULL;

  g_return_val_if_fail (FOUNDRY_IS_BUILD_PROGRESS (self), NULL);

  store = g_list_store_new (G_TYPE_FILE);

  for (guint i = 0; i < self->artifacts->len; i++)
    g_list_store_append (store, g_ptr_array_index (self->artifacts, i));

  return g_object_ref (G_LIST_MODEL (store));
}
