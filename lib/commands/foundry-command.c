/* foundry-command.c
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

#include "foundry-command-provider.h"
#include "foundry-command-private.h"
#include "foundry-debug.h"
#include "foundry-process-launcher.h"

typedef struct
{
  GWeakRef provider_wr;
  char **argv;
  char **environ;
  char *id;
  char *cwd;
  char *name;
} FoundryCommandPrivate;

enum {
  PROP_0,
  PROP_ARGV,
  PROP_CWD,
  PROP_ENVIRON,
  PROP_ID,
  PROP_NAME,
  PROP_PROVIDER,
  N_PROPS
};

G_DEFINE_TYPE (FoundryCommand, foundry_command, FOUNDRY_TYPE_CONTEXTUAL)

static GParamSpec *properties[N_PROPS];

static DexFuture *
foundry_command_real_prepare (FoundryCommand         *command,
                              FoundryProcessLauncher *launcher)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (command);

  g_assert (FOUNDRY_IS_MAIN_THREAD ());
  g_assert (FOUNDRY_IS_COMMAND (command));
  g_assert (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  if (priv->cwd != NULL)
    foundry_process_launcher_set_cwd (launcher, priv->cwd);

  if (priv->argv != NULL)
    foundry_process_launcher_set_argv (launcher, (const char * const *)priv->argv);

  if (priv->environ != NULL)
    foundry_process_launcher_set_environ (launcher, (const char * const *)priv->environ);

  return dex_future_new_true ();
}

static void
foundry_command_finalize (GObject *object)
{
  FoundryCommand *self = (FoundryCommand *)object;
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_weak_ref_clear (&priv->provider_wr);

  g_clear_pointer (&priv->argv, g_strfreev);
  g_clear_pointer (&priv->environ, g_strfreev);
  g_clear_pointer (&priv->cwd, g_free);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (foundry_command_parent_class)->finalize (object);
}

static void
foundry_command_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  FoundryCommand *self = FOUNDRY_COMMAND (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      g_value_take_boxed (value, foundry_command_dup_argv (self));
      break;

    case PROP_CWD:
      g_value_take_string (value, foundry_command_dup_cwd (self));
      break;

    case PROP_ENVIRON:
      g_value_take_boxed (value, foundry_command_dup_environ (self));
      break;

    case PROP_ID:
      g_value_take_string (value, foundry_command_dup_id (self));
      break;

    case PROP_NAME:
      g_value_take_string (value, foundry_command_dup_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_command_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  FoundryCommand *self = FOUNDRY_COMMAND (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      foundry_command_set_argv (self, g_value_get_boxed (value));
      break;

    case PROP_CWD:
      foundry_command_set_cwd (self, g_value_get_string (value));
      break;

    case PROP_ENVIRON:
      foundry_command_set_environ (self, g_value_get_boxed (value));
      break;

    case PROP_ID:
      foundry_command_set_id (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      foundry_command_set_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
foundry_command_class_init (FoundryCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = foundry_command_finalize;
  object_class->get_property = foundry_command_get_property;
  object_class->set_property = foundry_command_set_property;

  klass->prepare = foundry_command_real_prepare;

  properties[PROP_ARGV] =
    g_param_spec_boxed ("argv", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_CWD] =
    g_param_spec_string ("cwd", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ENVIRON] =
    g_param_spec_boxed ("environ", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_PROVIDER] =
    g_param_spec_object ("provider", NULL, NULL,
                         FOUNDRY_TYPE_COMMAND_PROVIDER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
foundry_command_init (FoundryCommand *self)
{
}

/**
 * foundry_command_dup_argv:
 * @self: a [class@Foundry.Command]
 *
 * Returns: (transfer full) (nullable): a string array of arguments for
 *   the command to run.
 */
char **
foundry_command_dup_argv (FoundryCommand *self)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), NULL);

  return g_strdupv (priv->argv);
}

void
foundry_command_set_argv (FoundryCommand     *self,
                          const char * const *argv)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);
  char **copy;

  g_return_if_fail (FOUNDRY_IS_COMMAND (self));

  if (argv == (const char * const *)priv->argv)
    return;

  copy = g_strdupv ((char **)argv);
  g_strfreev (priv->argv);
  priv->argv = copy;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ARGV]);
}

/**
 * foundry_command_dup_environ:
 * @self: a [class@Foundry.Command]
 *
 * Returns: (transfer full) (nullable): a string array containing the
 *   environment of %NULL.
 */
char **
foundry_command_dup_environ (FoundryCommand *self)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), NULL);

  return g_strdupv (priv->environ);
}

void
foundry_command_set_environ (FoundryCommand     *self,
                             const char * const *environ)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);
  char **copy;

  g_return_if_fail (FOUNDRY_IS_COMMAND (self));

  if (environ == (const char * const *)priv->environ)
    return;

  copy = g_strdupv ((char **)environ);
  g_strfreev (priv->environ);
  priv->environ = copy;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENVIRON]);
}

char *
foundry_command_dup_cwd (FoundryCommand *self)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), NULL);

  return g_strdup (priv->cwd);
}

void
foundry_command_set_cwd (FoundryCommand *self,
                         const char     *cwd)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_COMMAND (self));

  if (g_set_str (&priv->cwd, cwd))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CWD]);
}

char *
foundry_command_dup_id (FoundryCommand *self)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), NULL);

  return g_strdup (priv->id);
}

void
foundry_command_set_id (FoundryCommand *self,
                        const char     *id)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_COMMAND (self));

  if (g_set_str (&priv->id, id))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
}

char *
foundry_command_dup_name (FoundryCommand *self)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), NULL);

  return g_strdup (priv->name);
}

void
foundry_command_set_name (FoundryCommand *self,
                          const char     *name)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_if_fail (FOUNDRY_IS_COMMAND (self));

  if (g_set_str (&priv->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

/**
 * foundry_command_can_default:
 * @self: a [class@Foundry.Command]
 * @priority: (out) (nullable): a location for the priority, or %NULL
 *
 * Checks to see if @self is suitable to be run as the default command when
 * running a project. The priority indicates if it should take priority over
 * other commands which can be default. The highest priority value wins.
 *
 * Returns: %TRUE if @self can be the default command
 */
gboolean
foundry_command_can_default (FoundryCommand *self,
                             guint          *priority)
{
  guint local;

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), FALSE);

  if (priority == NULL)
    priority = &local;

  if (FOUNDRY_COMMAND_GET_CLASS (self)->can_default)
    return FOUNDRY_COMMAND_GET_CLASS (self)->can_default (self, priority);

  return FALSE;
}

DexFuture *
foundry_command_prepare (FoundryCommand         *self,
                         FoundryProcessLauncher *launcher)
{
  dex_return_error_if_fail (FOUNDRY_IS_COMMAND (self));
  dex_return_error_if_fail (FOUNDRY_IS_PROCESS_LAUNCHER (launcher));

  return FOUNDRY_COMMAND_GET_CLASS (self)->prepare (self, launcher);
}

FoundryCommandProvider *
_foundry_command_dup_provider (FoundryCommand *self)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);

  g_return_val_if_fail (FOUNDRY_IS_COMMAND (self), NULL);

  return g_weak_ref_get (&priv->provider_wr);
}

void
_foundry_command_set_provider (FoundryCommand         *self,
                               FoundryCommandProvider *provider)
{
  FoundryCommandPrivate *priv = foundry_command_get_instance_private (self);
  g_autoptr(FoundryCommandProvider) previous = NULL;

  g_return_if_fail (FOUNDRY_IS_COMMAND (self));
  g_return_if_fail (!provider || FOUNDRY_IS_COMMAND_PROVIDER (provider));

  previous = g_weak_ref_get (&priv->provider_wr);

  if (previous == provider)
    return;

  g_weak_ref_set (&priv->provider_wr, provider);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROVIDER]);
}
