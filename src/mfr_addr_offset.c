/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmbus_io.h"
#include "mfr_addr_offset.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void
usage_addr_offset(void) {
  fprintf(stderr,
"addr-offset get\n"
"addr-offset set --raw 0xNN\n"
  );
}

int
cmd_addr_offset(int fd, int argc, char *const *argv, int pretty) {
  if (argc == 0) {
    usage_addr_offset();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int v = pmbus_rd_byte(fd, MFR_OFFSET_ADDRESS);
    if (v < 0) {
      perror("MFR_OFFSET_ADDRESS");
      return 1;
    }
    json_t *o = json_object();
    json_object_set_new(o, "MFR_OFFSET_ADDRESS", json_integer(v));

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    const char *raw = NULL;
    for (int i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--raw") && i + 1 < argc)
        raw = argv[++i];
    }
    if (!raw) {
      usage_addr_offset();
      return 2;
    }
    char *end = NULL;
    errno = 0;
    long v = strtol(raw, &end, 0);
    if (errno || !end || *end || v < 0 || v > 0xFF) {
      usage_addr_offset();
      return 2;
    }
    if (pmbus_wr_byte(fd, MFR_OFFSET_ADDRESS, (uint8_t) v) < 0) {
      perror("MFR_OFFSET_ADDRESS write");
      return 1;
    }
    int rb = pmbus_rd_byte(fd, MFR_OFFSET_ADDRESS);
    if (rb < 0) {
      perror("MFR_OFFSET_ADDRESS readback");
      return 1;
    }
    json_t *o = json_object();
    json_object_set_new(o, "changed", json_true());
    json_object_set_new(o, "MFR_OFFSET_ADDRESS", json_integer(rb));

    json_print_or_pretty(o, pretty);

    return 0;
  }

  usage_addr_offset();

  return 2;
}
