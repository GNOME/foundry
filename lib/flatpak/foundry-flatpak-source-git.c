/* foundry-flatpak-source-git.c
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

#include "foundry-flatpak-source-git.h"
#include "foundry-flatpak-source-private.h"

struct _FoundryFlatpakSourceGit
{
  FoundryFlatpakSource  parent_instance;
  char                 *url;
  char                 *path;
  char                 *branch;
  char                 *tag;
  char                 *commit;
  char                 *orig_ref;
  char                 *default_branch_name;
  guint                 disable_fsckobjects : 1;
  guint                 disable_shallow_clone : 1;
  guint                 disable_submodules : 1;
};

enum {
  PROP_0,
  PROP_URL,
  PROP_PATH,
  PROP_BRANCH,
  PROP_TAG,
  PROP_COMMIT,
  PROP_DISABLE_FSCKOBJECTS,
  PROP_DISABLE_SHALLOW_CLONE,
  PROP_DISABLE_SUBMODULES,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (FoundryFlatpakSourceGit, foundry_flatpak_source_git, FOUNDRY_TYPE_FLATPAK_SOURCE)

static void
foundry_flatpak_source_git_finalize (GObject *object)
{
  FoundryFlatpakSourceGit *self = (FoundryFlatpakSourceGit *)object;

  g_clear_pointer (&self->url, g_free);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->branch, g_free);
  g_clear_pointer (&self->tag, g_free);
  g_clear_pointer (&self->commit, g_free);
  g_clear_pointer (&self->orig_ref, g_free);
  g_clear_pointer (&self->default_branch_name, g_free);

  G_OBJECT_CLASS (foundry_flatpak_source_git_parent_class)->finalize (object);
}

static void
foundry_flatpak_source_git_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  FoundryFlatpakSourceGit *self = FOUNDRY_FLATPAK_SOURCE_GIT (object);

  switch (prop_id)
    {
    case PROP_URL:
      g_value_set_string (value, self->url);
      break;

    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    case PROP_BRANCH:
      g_value_set_string (value, self->branch);
      break;

    case PROP_TAG:
      g_value_set_string (value, self->tag);
      break;

    case PROP_COMMIT:
      g_value_set_string (value, self->commit);
      break;

    case PROP_DISABLE_FSCKOBJECTS:
      g_value_set_boolean (value, self->disable_fsckobjects);
      break;

    case PROP_DISABLE_SHALLOW_CLONE:
      g_value_set_boolean (value, self->disable_shallow_clone);
      break;

    case PROP_DISABLE_SUBMODULES:
      g_value_set_boolean (value, self->disable_submodules);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_flatpak_source_git_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  FoundryFlatpakSourceGit *self = FOUNDRY_FLATPAK_SOURCE_GIT (object);

  switch (prop_id)
    {
    case PROP_URL:
      g_set_str (&self->url, g_value_get_string (value));
      break;

    case PROP_PATH:
      g_set_str (&self->path, g_value_get_string (value));
      break;

    case PROP_BRANCH:
      g_set_str (&self->branch, g_value_get_string (value));
      break;

    case PROP_TAG:
      g_set_str (&self->tag, g_value_get_string (value));
      break;

    case PROP_COMMIT:
      g_set_str (&self->commit, g_value_get_string (value));
      break;

    case PROP_DISABLE_FSCKOBJECTS:
      self->disable_fsckobjects = g_value_get_boolean (value);
      break;

    case PROP_DISABLE_SHALLOW_CLONE:
      self->disable_shallow_clone = g_value_get_boolean (value);
      break;

    case PROP_DISABLE_SUBMODULES:
      self->disable_submodules = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_flatpak_source_git_class_init (FoundryFlatpakSourceGitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  FoundryFlatpakSourceClass *source_class = FOUNDRY_FLATPAK_SOURCE_CLASS (klass);

  object_class->finalize = foundry_flatpak_source_git_finalize;
  object_class->get_property = foundry_flatpak_source_git_get_property;
  object_class->set_property = foundry_flatpak_source_git_set_property;

  source_class->type = "git";

  g_object_class_install_property (object_class,
                                   PROP_URL,
                                   g_param_spec_string ("url",
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_PATH,
                                   g_param_spec_string ("path",
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BRANCH,
                                   g_param_spec_string ("branch",
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_TAG,
                                   g_param_spec_string ("tag",
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_COMMIT,
                                   g_param_spec_string ("commit",
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_DISABLE_FSCKOBJECTS,
                                   g_param_spec_boolean ("disable-fsckobjects",
                                                         NULL,
                                                         NULL,
                                                         FALSE,
                                                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_DISABLE_SHALLOW_CLONE,
                                   g_param_spec_boolean ("disable-shallow-clone",
                                                         NULL,
                                                         NULL,
                                                         FALSE,
                                                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_DISABLE_SUBMODULES,
                                   g_param_spec_boolean ("disable-submodules",
                                                         NULL,
                                                         NULL,
                                                         FALSE,
                                                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
}

static void
foundry_flatpak_source_git_init (FoundryFlatpakSourceGit *self)
{
}
