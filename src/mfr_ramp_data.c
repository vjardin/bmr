/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmbus_io.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void
usage_ramp_data(void) {
  fprintf(stderr,
"ramp-data\n"
  );
}

int
cmd_ramp_data(int fd, int argc, char *const *argv, int pretty) {
  (void) argv;

  if (argc > 0) {
    usage_ramp_data();
    return 2;
  }

  uint8_t buf[255];
  int n = pmbus_rd_block(fd, MFR_GET_RAMP_DATA, buf, (int) sizeof(buf));
  if (n < 0) {
    perror("MFR_GET_RAMP_DATA");
    return 1;
  }

  char *hex = (char *) malloc((size_t) n * 2 + 1);
  for (int i = 0; i < n; i++)
    sprintf(hex + 2 * i, "%02X", buf[i]);
  hex[n * 2] = '\0';

  json_t *o = json_object();
  json_object_set_new(o, "len", json_integer(n));
  json_object_set_new(o, "hex", json_string(hex));
  free(hex);

  json_print_or_pretty(o, pretty);

  return 0;
}
