/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmbus_io.h"
#include "util_json.h"
#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void
usage_salert(void) {
  fprintf(stderr,
"salert get\n"
"salert set --raw 0xNN\n"
  );
}

int
cmd_salert(int fd, int argc, char *const *argv, int pretty) {
  if (argc == 0) {
    usage_salert();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int v = pmbus_rd_byte(fd, PMBUS_SMBALERT_MASK);
    if (v < 0) {
      perror("SMBALERT_MASK");
      return 1;
    }
    json_t *o = json_object();
    json_object_set_new(o, "SMBALERT_MASK", json_integer(v));

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
      usage_salert();
      return 2;
    }
    char *end = NULL;
    errno = 0;
    long v = strtol(raw, &end, 0);
    if (errno || !end || *end || v < 0 || v > 0xFF) {
      usage_salert();
      return 2;
    }
    if (pmbus_wr_byte(fd, PMBUS_SMBALERT_MASK, (uint8_t) v) < 0) {
      perror("SMBALERT_MASK write");
      return 1;
    }
    int rb = pmbus_rd_byte(fd, PMBUS_SMBALERT_MASK);
    if (rb < 0) {
      perror("SMBALERT_MASK readback");
      return 1;
    }
    json_t *o = json_object();
    json_object_set_new(o, "changed", json_true());
    json_object_set_new(o, "SMBALERT_MASK", json_integer(rb));

    json_print_or_pretty(o, pretty);

    return 0;
  }

  usage_salert();

  return 2;
}
