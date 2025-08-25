/* foundry-tweak-tree.c
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

#include "foundry-internal-tweak.h"
#include "foundry-tweak.h"
#include "foundry-tweak-info-private.h"
#include "foundry-tweak-path.h"
#include "foundry-tweak-tree.h"

typedef struct _Registration
{
  guint              registration;
  FoundryTweakPath  *path;
  const char        *gettext_domain;
  FoundryTweakInfo **infos;
  guint              n_infos;
} Registration;

struct _FoundryTweakTree
{
  GObject  parent_instance;
  GArray  *registrations;
  guint    last_seq;
};

G_DEFINE_FINAL_TYPE (FoundryTweakTree, foundry_tweak_tree, G_TYPE_OBJECT)

static void
clear_registration (Registration *registration)
{
  g_clear_pointer (&registration->path, foundry_tweak_path_free);
  for (guint i = 0; i < registration->n_infos; i++)
    g_clear_pointer (&registration->infos[i], foundry_tweak_info_unref);
  g_clear_pointer (&registration->infos, g_free);
}

static void
foundry_tweak_tree_finalize (GObject *object)
{
  G_OBJECT_CLASS (foundry_tweak_tree_parent_class)->finalize (object);
}

static void
foundry_tweak_tree_class_init (FoundryTweakTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_tweak_tree_finalize;
}

static void
foundry_tweak_tree_init (FoundryTweakTree *self)
{
  self->registrations = g_array_new (FALSE, FALSE, sizeof (Registration));
  g_array_set_clear_func (self->registrations, (GDestroyNotify)clear_registration);
}

FoundryTweakTree *
foundry_tweak_tree_new (FoundryContext *context)
{
  return g_object_new (FOUNDRY_TYPE_TWEAK_TREE, NULL);
}

static int
sort_by_path (gconstpointer a,
              gconstpointer b)
{
  const Registration *reg_a = a;
  const Registration *reg_b = b;

  return foundry_tweak_path_compare (reg_a->path, reg_b->path);
}

guint
foundry_tweak_tree_register (FoundryTweakTree       *self,
                             const char             *gettext_domain,
                             const char             *path,
                             const FoundryTweakInfo *infos,
                             guint                   n_infos,
                             const char * const     *environment)
{
  Registration reg = {0};

  g_return_val_if_fail (FOUNDRY_IS_TWEAK_TREE (self), 0);
  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (infos != NULL || n_infos == 0, 0);

  if (n_infos == 0)
    return 0;

  reg.registration = ++self->last_seq;
  reg.gettext_domain = g_intern_string (gettext_domain);
  reg.path = foundry_tweak_path_new (path);
  reg.infos = g_new0 (FoundryTweakInfo *, n_infos);
  reg.n_infos = n_infos;

  for (guint i = 0; i < n_infos; i++)
    reg.infos[i] = foundry_tweak_info_expand (&infos[i], environment);

  g_array_append_val (self->registrations, reg);
  g_array_sort (self->registrations, sort_by_path);

  return reg.registration;
}

void
foundry_tweak_tree_unregister (FoundryTweakTree *self,
                               guint             registration)
{
  g_return_if_fail (FOUNDRY_IS_TWEAK_TREE (self));
  g_return_if_fail (registration != 0);

  for (guint i = 0; i < self->registrations->len; i++)
    {
      const Registration *reg = &g_array_index (self->registrations, Registration, i);

      if (reg->registration == registration)
        {
          g_array_remove_index (self->registrations, i);
          break;
        }
    }
}

GListModel *
foundry_tweak_tree_list (FoundryTweakTree *self,
                         const char       *path)
{
  g_autoptr(FoundryTweakPath) real_path = NULL;
  g_autoptr(GListStore) store = NULL;

  g_return_val_if_fail (FOUNDRY_IS_TWEAK_TREE (self), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (g_str_has_suffix (path, "/"), NULL);

  real_path = foundry_tweak_path_new (path);
  store = g_list_store_new (G_TYPE_OBJECT);

  for (guint i = 0; i < self->registrations->len; i++)
    {
      const Registration *reg = &g_array_index (self->registrations, Registration, i);
      int depth = foundry_tweak_path_compute_depth (real_path, reg->path);

      if (depth < 0)
        continue;

      for (guint j = 0; j < reg->n_infos; j++)
        {
          FoundryTweakInfo *info = reg->infos[j];
          g_autoptr(FoundryTweakPath) info_path = foundry_tweak_path_push (reg->path, info->subpath);
          int info_depth = foundry_tweak_path_compute_depth (real_path, info_path);
          g_autoptr(FoundryTweak) tweak = NULL;

          if (info_depth > 1)
            continue;

          tweak = foundry_internal_tweak_new (reg->gettext_domain,
                                              foundry_tweak_info_ref (info),
                                              foundry_tweak_path_dup_path (info_path));

          if (tweak != NULL)
            g_list_store_append (store, tweak);
        }
    }

  if (g_list_model_get_n_items (G_LIST_MODEL (store)) == 0)
    return NULL;

  return G_LIST_MODEL (g_steal_pointer (&store));
}
