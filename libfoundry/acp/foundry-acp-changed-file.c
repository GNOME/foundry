/* foundry-acp-changed-file.c
 *
 * Copyright 2026 Christian Hergert
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-acp-changed-file.h"
#include "foundry-acp-enums.h"

struct _FoundryAcpChangedFile
{
  GObject parent_instance;
  char *path;
  char *last_event_id;
  FoundryAcpChangedFileKind kind;
  FoundryAcpChangedFileFlags flags;
};

G_DEFINE_FINAL_TYPE (FoundryAcpChangedFile, foundry_acp_changed_file, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PATH,
  PROP_KIND,
  PROP_LAST_EVENT_ID,
  PROP_FLAGS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
foundry_acp_changed_file_finalize (GObject *object)
{
  FoundryAcpChangedFile *self = (FoundryAcpChangedFile *)object;

  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->last_event_id, g_free);

  G_OBJECT_CLASS (foundry_acp_changed_file_parent_class)->finalize (object);
}

static void
foundry_acp_changed_file_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  FoundryAcpChangedFile *self = FOUNDRY_ACP_CHANGED_FILE (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    case PROP_KIND:
      g_value_set_enum (value, self->kind);
      break;

    case PROP_LAST_EVENT_ID:
      g_value_set_string (value, self->last_event_id);
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, self->flags);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_acp_changed_file_class_init (FoundryAcpChangedFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_acp_changed_file_finalize;
  object_class->get_property = foundry_acp_changed_file_get_property;

  properties[PROP_PATH] =
    g_param_spec_string ("path", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_KIND] =
    g_param_spec_enum ("kind", NULL, NULL,
                       FOUNDRY_TYPE_ACP_CHANGED_FILE_KIND,
                       FOUNDRY_ACP_CHANGED_FILE_UNKNOWN,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_LAST_EVENT_ID] =
    g_param_spec_string ("last-event-id", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_FLAGS] =
    g_param_spec_flags ("flags", NULL, NULL,
                        FOUNDRY_TYPE_ACP_CHANGED_FILE_FLAGS,
                        FOUNDRY_ACP_CHANGED_FILE_NONE,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_acp_changed_file_init (FoundryAcpChangedFile *self)
{
}

/**
 * foundry_acp_changed_file_new:
 * @path: a project-relative or absolute file path
 * @kind: the kind of change
 * @last_event_id: (nullable): the last event known to reference @path
 * @flags: additional VCS status flags
 *
 * Creates a new changed-file entry for ACP monitoring.
 *
 * Returns: (transfer full): a [class@Foundry.AcpChangedFile]
 *
 * Since: 1.2
 */
FoundryAcpChangedFile *
foundry_acp_changed_file_new (const char                 *path,
                              FoundryAcpChangedFileKind   kind,
                              const char                 *last_event_id,
                              FoundryAcpChangedFileFlags  flags)
{
  FoundryAcpChangedFile *self;

  g_return_val_if_fail (path != NULL, NULL);

  self = g_object_new (FOUNDRY_TYPE_ACP_CHANGED_FILE, NULL);
  self->path = g_strdup (path);
  self->kind = kind;
  self->last_event_id = g_strdup (last_event_id);
  self->flags = flags;

  return self;
}

/**
 * foundry_acp_changed_file_dup_path:
 * @self: a [class@Foundry.AcpChangedFile]
 *
 * Returns: (transfer full): the changed file path
 *
 * Since: 1.2
 */
char *
foundry_acp_changed_file_dup_path (FoundryAcpChangedFile *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CHANGED_FILE (self), NULL);

  return g_strdup (self->path);
}

/**
 * foundry_acp_changed_file_get_kind:
 * @self: a [class@Foundry.AcpChangedFile]
 *
 * Returns: the kind of change
 *
 * Since: 1.2
 */
FoundryAcpChangedFileKind
foundry_acp_changed_file_get_kind (FoundryAcpChangedFile *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CHANGED_FILE (self), FOUNDRY_ACP_CHANGED_FILE_UNKNOWN);

  return self->kind;
}

/**
 * foundry_acp_changed_file_dup_last_event_id:
 * @self: a [class@Foundry.AcpChangedFile]
 *
 * Returns: (transfer full) (nullable): the last event identifier known to
 *   reference this file
 *
 * Since: 1.2
 */
char *
foundry_acp_changed_file_dup_last_event_id (FoundryAcpChangedFile *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CHANGED_FILE (self), NULL);

  return g_strdup (self->last_event_id);
}

/**
 * foundry_acp_changed_file_get_flags:
 * @self: a [class@Foundry.AcpChangedFile]
 *
 * Returns: status flags for the changed file
 *
 * Since: 1.2
 */
FoundryAcpChangedFileFlags
foundry_acp_changed_file_get_flags (FoundryAcpChangedFile *self)
{
  g_return_val_if_fail (FOUNDRY_IS_ACP_CHANGED_FILE (self), FOUNDRY_ACP_CHANGED_FILE_NONE);

  return self->flags;
}
