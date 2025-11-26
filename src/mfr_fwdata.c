/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "mfr_fwdata.h"
#include "util_json.h"

#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>

int
cmd_fwdata(int fd, int pretty) {
  uint8_t b[32];

  int n = pmbus_rd_block(fd, MFR_FIRMWARE_DATA, b, sizeof b);
  if (n < 0) {
    perror("MFR_FIRMWARE_DATA");
    return 1;
  }

  json_t *o = json_object();
  json_object_set_new(o, "len", json_integer(n));
  json_add_hex_ascii(o, "hex", b, (size_t)n);

  /* Extract printable runs >= 3 */
  json_t *runs = json_array();
  size_t run = 0, start = 0;
  for (size_t i = 0; i < (size_t)n; i++) {
    if (b[i] >= 32 && b[i] <= 126) {
      if (run == 0)
        start = i;
      run++;
    } else {
      if (run >= 3)
        json_array_append_new(runs, json_stringn((char *) &b[start], run));
      run = 0;
    }
  }

  if (run >= 3)
    json_array_append_new(runs, json_stringn((char *) &b[start], run));
  json_object_set_new(o, "ascii_runs", runs);

  json_print_or_pretty(o, pretty);

  return 0;
}
