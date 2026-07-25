/* foundry-ci-artifact.c
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "foundry-ci-artifact.h"

/**
 * FoundryCiArtifact:
 *
 * A file or directory produced by a continuous integration run.
 *
 * Since: 1.2
 */

struct _FoundryCiArtifact
{
  GObject                parent_instance;
  char                  *name;
  GFile                 *file;
  FoundryCiArtifactKind  kind;
};

enum {
  PROP_0,
  PROP_FILE,
  PROP_KIND,
  PROP_NAME,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryCiArtifact, foundry_ci_artifact, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
foundry_ci_artifact_finalize (GObject *object)
{
  FoundryCiArtifact *self = FOUNDRY_CI_ARTIFACT (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (foundry_ci_artifact_parent_class)->finalize (object);
}

static void
foundry_ci_artifact_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  FoundryCiArtifact *self = FOUNDRY_CI_ARTIFACT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_KIND:
      g_value_set_enum (value, self->kind);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
foundry_ci_artifact_class_init (FoundryCiArtifactClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_ci_artifact_finalize;
  object_class->get_property = foundry_ci_artifact_get_property;

  /**
   * FoundryCiArtifact:file:
   *
   * The artifact location.
   *
   * Since: 1.2
   */
  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiArtifact:kind:
   *
   * The kind of artifact.
   *
   * Since: 1.2
   */
  properties[PROP_KIND] =
    g_param_spec_enum ("kind", NULL, NULL,
                       FOUNDRY_TYPE_CI_ARTIFACT_KIND,
                       FOUNDRY_CI_ARTIFACT_KIND_FILE,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * FoundryCiArtifact:name:
   *
   * The user-visible artifact name.
   *
   * Since: 1.2
   */
  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_ci_artifact_init (FoundryCiArtifact *self)
{
}

/**
 * foundry_ci_artifact_new:
 * @name: a user-visible artifact name
 * @file: the artifact location
 * @kind: the kind of artifact
 *
 * Creates a description of an artifact produced by a CI run.
 *
 * Returns: (transfer full): a new [class@Foundry.CiArtifact]
 *
 * Since: 1.2
 */
FoundryCiArtifact *
foundry_ci_artifact_new (const char            *name,
                         GFile                 *file,
                         FoundryCiArtifactKind  kind)
{
  FoundryCiArtifact *self;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  self = g_object_new (FOUNDRY_TYPE_CI_ARTIFACT, NULL);
  self->name = g_strdup (name);
  self->file = g_object_ref (file);
  self->kind = kind;

  return self;
}

/**
 * foundry_ci_artifact_dup_name:
 * @self: a [class@Foundry.CiArtifact]
 *
 * Gets the user-visible name of the artifact.
 *
 * Returns: (transfer full): the artifact name
 *
 * Since: 1.2
 */
char *
foundry_ci_artifact_dup_name (FoundryCiArtifact *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_ARTIFACT (self), NULL);

  return g_strdup (self->name);
}

/**
 * foundry_ci_artifact_dup_file:
 * @self: a [class@Foundry.CiArtifact]
 *
 * Returns: (transfer full): a [iface@Gio.File]
 *
 * Since: 1.2
 */
GFile *
foundry_ci_artifact_dup_file (FoundryCiArtifact *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_ARTIFACT (self), NULL);

  return g_object_ref (self->file);
}

/**
 * foundry_ci_artifact_get_kind:
 * @self: a [class@Foundry.CiArtifact]
 *
 * Gets the kind of artifact.
 *
 * Returns: the artifact kind
 *
 * Since: 1.2
 */
FoundryCiArtifactKind
foundry_ci_artifact_get_kind (FoundryCiArtifact *self)
{
  g_return_val_if_fail (FOUNDRY_IS_CI_ARTIFACT (self), 0);

  return self->kind;
}
