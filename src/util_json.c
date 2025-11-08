#include "util_json.h"
#include <stdio.h>
#include <stdlib.h>

void
json_print_or_pretty(json_t *o, int pretty) {
  if (!o)
    return;

  char *s = json_dumps(o, pretty ? JSON_INDENT(2) | JSON_SORT_KEYS : JSON_SORT_KEYS);

  puts(s);
  free(s);

  json_decref(o);
}
