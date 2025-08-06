{{include "license.c"}}

#include "config.h"

#include "{{file_base}}.h"

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
G_DEFINE_FINAL_TYPE ({{Prefix}}{{Class}}, {{prefix_}}_{{class_}}, {{PARENT_TYPE}})
{{else}}
G_DEFINE_TYPE_WITH_PRIVATE ({{Prefix}}{{Class}}, {{prefix_}}_{{class_}}, {{PARENT_TYPE}})
{{end}}

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
{{prefix_}}_{{class_}}_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), {{PREFIX}}_TYPE_{{CLASS}});

  G_OBJECT_CLASS ({{prefix_}}_{{class_}}_parent_class)->dispose (object);
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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = {{prefix_}}_{{class_}}_dispose;
  object_class->get_property = {{prefix_}}_{{class_}}_get_property;
  object_class->set_property = {{prefix_}}_{{class_}}_set_property;

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/ui/{{file_base}}.ui");
}

static void
{{prefix_}}_{{class_}}_init ({{Prefix}}{{Class}} *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

{{if final}}
GtkWidget *
{{prefix_}}_{{class_}}_new (void)
{
  return g_object_new ({{PREFIX}}_TYPE_{{CLASS}}, NULL);
}
{{end}}
