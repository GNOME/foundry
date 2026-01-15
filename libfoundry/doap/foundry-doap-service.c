/* foundry-doap-service.c
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

#include "foundry-doap-file.h"
#include "foundry-doap-service.h"
#include "foundry-file.h"

struct _FoundryDoapService
{
  FoundryService  parent_instance;
  FoundryDoapFile *doap_file;
};

struct _FoundryDoapServiceClass
{
  FoundryServiceClass parent_class;
};

G_DEFINE_FINAL_TYPE (FoundryDoapService, foundry_doap_service, FOUNDRY_TYPE_SERVICE)

enum {
  PROP_0,
  PROP_DOAP_FILE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_doap_service_start_fiber (gpointer data)
{
  FoundryDoapService *self = data;
  g_autoptr(FoundryContext) context = NULL;
  g_autoptr(GPtrArray) files = NULL;
  g_autoptr(GFile) project_dir = NULL;

  g_assert (FOUNDRY_IS_DOAP_SERVICE (self));

  context = foundry_contextual_dup_context (FOUNDRY_CONTEXTUAL (self));
  project_dir = foundry_context_dup_project_directory (context);

  /* Ignore if this isn't a real project */
  if (foundry_context_is_shared (context))
    return dex_future_new_true ();

  g_debug ("Searching `%s` for *.doap project file",
           g_file_peek_path (project_dir));

  /* Find *.doap files so we can parse them */
  if (!(files = dex_await_boxed (foundry_file_find_with_depth (project_dir, "*.doap", 1), NULL)))
    return dex_future_new_true ();

  for (guint i = 0; i < files->len; i++)
    {
      GFile *file = g_ptr_array_index (files, i);
      g_autoptr(FoundryDoapFile) doap_file = NULL;

      if ((doap_file = dex_await_object (foundry_doap_file_new_from_file (file), NULL)))
        {
          g_autofree char *basename = g_file_get_basename (file);
          const char *name = foundry_doap_file_get_name (doap_file);

          g_debug ("Discovered project name `%s` from `%s`.",
                   name, basename);

          if (g_set_object (&self->doap_file, doap_file))
            {
              g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DOAP_FILE]);
              foundry_context_set_title (context, name);
            }

          break;
        }
    }

  return dex_future_new_true ();
}

static DexFuture *
foundry_doap_service_start (FoundryService *service)
{
  return dex_scheduler_spawn (NULL, 0,
                              foundry_doap_service_start_fiber,
                              g_object_ref (service),
                              g_object_unref);
}

static DexFuture *
foundry_doap_service_stop (FoundryService *service)
{
  FoundryDoapService *self = (FoundryDoapService *)service;

  g_assert (FOUNDRY_IS_DOAP_SERVICE (self));

  g_clear_object (&self->doap_file);

  return dex_future_new_true ();
}

static void
foundry_doap_service_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FoundryDoapService *self = FOUNDRY_DOAP_SERVICE (object);

  switch (prop_id)
    {
    case PROP_DOAP_FILE:
      g_value_take_object (value, foundry_doap_service_dup_doap_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_doap_service_class_init (FoundryDoapServiceClass *klass)
{
  FoundryServiceClass *service_class = FOUNDRY_SERVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = foundry_doap_service_get_property;

  service_class->start = foundry_doap_service_start;
  service_class->stop = foundry_doap_service_stop;

  properties[PROP_DOAP_FILE] =
    g_param_spec_object ("doap-file", NULL, NULL,
                         FOUNDRY_TYPE_DOAP_FILE,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_doap_service_init (FoundryDoapService *self)
{
}

/**
 * foundry_doap_service_dup_doap_file:
 * @self: a [class@Foundry.DoapService]
 *
 * Returns: (transfer full) (nullable): a [class@Foundry.DoapFile] or %NULL
 *
 * Since: 1.1
 */
FoundryDoapFile *
foundry_doap_service_dup_doap_file (FoundryDoapService *self)
{
  g_return_val_if_fail (FOUNDRY_IS_DOAP_SERVICE (self), NULL);

  if (self->doap_file)
    return g_object_ref (self->doap_file);

  return NULL;
}
