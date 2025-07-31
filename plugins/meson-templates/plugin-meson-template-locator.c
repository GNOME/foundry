/* plugin-meson-template-locator.c
 *
 * Copyright 2022-2025 Christian Hergert <chergert@redhat.com>
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

#include "line-reader-private.h"

#include "plugin-meson-template-locator.h"

struct _PluginMesonTemplateLocator
{
  TmplTemplateLocator parent_instance;
  GBytes *license_text;
};

G_DEFINE_FINAL_TYPE (PluginMesonTemplateLocator, plugin_meson_template_locator, TMPL_TYPE_TEMPLATE_LOCATOR)

enum {
  PROP_0,
  PROP_LICENSE_TEXT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

typedef struct _CommentInfo
{
  const char *suffix;
  const char *first_line;
  const char *line_prefix;
  const char *last_line;
} CommentInfo;

static const CommentInfo infos[] = {
  { ".c", "/*", " *", " */" },
  { ".h", "/*", " *", " */" },
  { ".vala", "/*", " *", " */" },

  { ".cc", "//", "//", "//" },
  { ".cpp", "//", "//", "//" },
  { ".hh", "//", "//", "//" },
  { ".hpp", "//", "//", "//" },
  { ".cs", "//", "//", "//" },
  { ".rs", "//", "//", "//" },

  { ".js", "/*", " *", " */" },

  { ".py", "#", "#", "#" },
};

static char *
format_header (const CommentInfo *info,
               GBytes            *text)
{
  LineReader reader;
  GString *str;
  char *line;
  gsize len;
  guint i = 0;

  if (text == NULL)
    return NULL;

  str = g_string_new (NULL);

  line_reader_init_from_bytes (&reader, text);

  while ((line = line_reader_next (&reader, &len)))
    {
      if (i == 0)
        g_string_append (str, info->first_line);
      else
        g_string_append (str, info->line_prefix);

      g_string_append_c (str, ' ');
      g_string_append_len (str, line, len);
      g_string_append_c (str, '\n');

      i++;
    }

  if (strcmp (info->line_prefix, info->last_line) != 0)
    g_string_append_printf (str, "%s\n", info->last_line);

  return g_string_free (str, FALSE);
}

static GInputStream *
plugin_meson_template_locator_locate (TmplTemplateLocator  *locator,
                                      const char           *path,
                                      GError              **error)
{
  PluginMesonTemplateLocator *self = (PluginMesonTemplateLocator *)locator;

  g_assert (PLUGIN_IS_MESON_TEMPLATE_LOCATOR (self));
  g_assert (path != NULL);

  if (self->license_text && g_str_has_prefix (path, "license."))
    {
      g_autofree char *stripped = NULL;

      if (g_str_has_suffix (path, ".in"))
        path = stripped = g_strndup (path, strlen (path) - 3);

      for (guint i = 0; i < G_N_ELEMENTS (infos); i++)
        {
          if (g_str_has_suffix (path, infos[i].suffix))
            {
              g_autofree char *header = format_header (&infos[i], self->license_text);
              gsize len = strlen (header);

              return g_memory_input_stream_new_from_data (g_steal_pointer (&header), len, g_free);
            }
        }

      /* We don't want to fail here just because we didn't have any
       * license text to expand into a header.
       */
      return g_memory_input_stream_new ();
    }

  return TMPL_TEMPLATE_LOCATOR_CLASS (plugin_meson_template_locator_parent_class)->locate (locator, path, error);
}

static void
plugin_meson_template_locator_dispose (GObject *object)
{
  PluginMesonTemplateLocator *self = (PluginMesonTemplateLocator *)object;

  g_clear_pointer (&self->license_text, g_free);

  G_OBJECT_CLASS (plugin_meson_template_locator_parent_class)->dispose (object);
}

static void
plugin_meson_template_locator_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  PluginMesonTemplateLocator *self = PLUGIN_MESON_TEMPLATE_LOCATOR (object);

  switch (prop_id)
    {
    case PROP_LICENSE_TEXT:
      g_value_take_boxed (value, plugin_meson_template_locator_dup_license_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_meson_template_locator_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  PluginMesonTemplateLocator *self = PLUGIN_MESON_TEMPLATE_LOCATOR (object);

  switch (prop_id)
    {
    case PROP_LICENSE_TEXT:
      plugin_meson_template_locator_set_license_text (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
plugin_meson_template_locator_class_init (PluginMesonTemplateLocatorClass *klass)
{
  TmplTemplateLocatorClass *locator_class = TMPL_TEMPLATE_LOCATOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = plugin_meson_template_locator_dispose;
  object_class->get_property = plugin_meson_template_locator_get_property;
  object_class->set_property = plugin_meson_template_locator_set_property;

  locator_class->locate = plugin_meson_template_locator_locate;

  properties[PROP_LICENSE_TEXT] =
    g_param_spec_boxed ("license-text", NULL, NULL,
                        G_TYPE_BYTES,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
plugin_meson_template_locator_init (PluginMesonTemplateLocator *self)
{
}

GBytes *
plugin_meson_template_locator_dup_license_text (PluginMesonTemplateLocator *self)
{
  g_return_val_if_fail (PLUGIN_IS_MESON_TEMPLATE_LOCATOR (self), NULL);

  return self->license_text ? g_bytes_ref (self->license_text) : NULL;
}

void
plugin_meson_template_locator_set_license_text (PluginMesonTemplateLocator *self,
                                                GBytes                     *license_text)
{
  g_return_if_fail (PLUGIN_IS_MESON_TEMPLATE_LOCATOR (self));

  if (self->license_text == license_text)
    return;

  if (license_text)
    g_bytes_ref (license_text);
  g_clear_pointer (&self->license_text, g_bytes_unref);
  self->license_text = license_text;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LICENSE_TEXT]);
}

TmplTemplateLocator *
plugin_meson_template_locator_new (void)
{
  return g_object_new (PLUGIN_TYPE_MESON_TEMPLATE_LOCATOR, NULL);
}
