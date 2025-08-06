{{include "license.h"}}

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define {{PREFIX}}_TYPE_{{CLASS}} ({{prefix_}}_{{class_}}_get_type())

G_DECLARE_{{if final}}FINAL{{else}}DERIVABLE{{end}}_TYPE ({{Prefix}}{{Class}}, {{prefix_}}_{{class_}}, {{PREFIX}}, {{CLASS}}, {{Parent}})

{{Prefix}}{{Class}} *{{prefix_}}_{{class_}}_new (void);

G_END_DECLS
