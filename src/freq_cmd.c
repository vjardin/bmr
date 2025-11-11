/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmbus_io.h"
#include "util_json.h"
#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void
usage_freq(void) {
  fprintf(stderr,
"freq get\n"
"freq set --raw 0xNNNN\n"
  );
}

int
cmd_freq(int fd, int argc, char *const *argv, int pretty) {
  if (argc == 0) {
    usage_freq();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int w = pmbus_rd_word(fd, PMBUS_FREQUENCY_SWITCH);
    if (w < 0) {
      perror("FREQUENCY_SWITCH");
      return 1;
    }
    json_t *o = json_object();
    json_object_set_new(o, "FREQUENCY_SWITCH_raw", json_integer((uint16_t) w));

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
      usage_freq();
      return 2;
    }

    char *end = NULL;
    errno = 0;
    long v = strtol(raw, &end, 0);
    if (errno || !end || *end || v < 0 || v > 0xFFFF) {
      usage_freq();
      return 2;
    }

    if (pmbus_wr_word(fd, PMBUS_FREQUENCY_SWITCH, (uint16_t) v) < 0) {
      perror("FREQUENCY_SWITCH write");
      return 1;
    }
    int rb = pmbus_rd_word(fd, PMBUS_FREQUENCY_SWITCH);
    if (rb < 0) {
      perror("FREQUENCY_SWITCH readback");
      return 1;
    }

    json_t *o = json_object();
    json_object_set_new(o, "changed", json_true());
    json_object_set_new(o, "FREQUENCY_SWITCH_raw", json_integer((uint16_t) rb));

    json_print_or_pretty(o, pretty);

    return 0;
  }

  usage_freq();

  return 2;
}
