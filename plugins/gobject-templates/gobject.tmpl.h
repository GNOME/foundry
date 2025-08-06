{{include "license.h"}}

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define {{PREFIX}}_TYPE_{{CLASS}} ({{prefix_}}_{{class_}}_get_type())

G_DECLARE_{{if final}}FINAL{{else}}DERIVABLE{{end}}_TYPE ({{Prefix}}{{Class}}, {{prefix_}}_{{class_}}, {{PREFIX}}, {{CLASS}}, {{Parent}})

{{if !final}}
struct _{{Prefix}}{{Class}}Class
{
  {{Parent}}Class parent_class;
};
{{end}}

{{if final}}
{{Prefix}}{{Class}} *{{prefix_}}_{{class_}}_new (void);
{{end}}

G_END_DECLS
