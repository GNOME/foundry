{{include "license.c"}}

#include "config.h"

{{if final}}
struct _{{Prefix}}{{Class}}
{
  {{Parent}} parent_instance;
};
{{else}}
typedef struct
{
  gpointer first_field;
} {{Prefix}}{{Class}}Private;
{{end}}

{{if final}}
G_DECLARE_FINAL_TYPE ({{Prefix}}{{Class}}, {{prefix_}}_{{class_}}, {{PARENT_TYPE}})
{{else}}
G_DECLARE_TYPE_WITH_PRIVATE ({{Prefix}}{{Class}}, {{prefix_}}_{{class_}}, {{PARENT_TYPE}})
{{end}}

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
{{prefix_}}_{{class_}}_finalize (GObject *object)
{
  G_OBJECT_CLASS ({{prefix_}}_{{class_}}_parent_class)->finalize (object);
}

static void
{{prefix_}}_{{class_}}_get_property (GObject    *object,
{{prefix_space}} {{class_space}}               guint       prop_id,
{{prefix_space}} {{class_space}}               GValue     *value,
{{prefix_space}} {{class_space}}               GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
{{prefix_}}_{{class_}}_set_property (GObject      *object,
{{prefix_space}} {{class_space}}               guint         prop_id,
{{prefix_space}} {{class_space}}               const GValue *value,
{{prefix_space}} {{class_space}}               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
{{prefix_}}_{{class_}}_class_init ({{Prefix}}{{Class}}Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = {{prefix_}}_{{class_}}_finalize;
  object_class->get_property = {{prefix_}}_{{class_}}_get_property;
  object_class->set_property = {{prefix_}}_{{class_}}_set_property;

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
{{prefix_}}_{{class_}}_init ({{Prefix}}{{Class}} *self)
{
}

{{if final}}
{{Prefix}}{{Class}} *
{{prefix_}}_{{class_}}_new (void)
{
  return g_object_new ({{PREFIX}}_TYPE_{{CLASS}}, NULL);
}
{{end}}
