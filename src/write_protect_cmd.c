/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmbus_io.h"
#include "write_protect_cmd.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define WP_NONE 0x00
#define WP_CTRL 0x40
#define WP_NVM  0x80
#define WP_ALL  0xFF

static void
usage_wp(void) {
  fprintf(stderr,
"write-protect get\n"
"write-protect set [--none|--ctrl|--nvm|--all] | --raw 0xNN\n"
  );
}

int
cmd_write_protect(int fd, int argc, char *const *argv, int pretty) {
  if (argc == 0) {
    usage_wp();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int v = pmbus_rd_byte(fd, PMBUS_WRITE_PROTECT);
    if (v < 0) {
      perror("WRITE_PROTECT");
      return 1;
    }
    json_t *o = json_object();
    json_object_set_new(o, "WRITE_PROTECT", json_integer(v));
    json_object_set_new(o, "none", json_boolean(v == WP_NONE));
    json_object_set_new(o, "ctrl", json_boolean(v == WP_CTRL));
    json_object_set_new(o, "nvm", json_boolean(v == WP_NVM));
    json_object_set_new(o, "all", json_boolean(v == WP_ALL));

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    int mode = -1;
    const char *raw = NULL;
    for (int i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--none"))
        mode = 1;
      else if (!strcmp(argv[i], "--ctrl"))
        mode = 2;
      else if (!strcmp(argv[i], "--nvm"))
        mode = 3;
      else if (!strcmp(argv[i], "--all"))
        mode = 4;
      else if (!strcmp(argv[i], "--raw") && i + 1 < argc)
        raw = argv[++i];
    }
    uint8_t vset = 0;
    if (raw) {
      char *end = NULL;
      errno = 0;
      long v = strtol(raw, &end, 0);
      if (errno || !end || *end || v < 0 || v > 0xFF) {
        usage_wp();
        return 2;
      }
      vset = (uint8_t) v;
    } else if (mode > 0) {
      vset = (mode == 1 ? WP_NONE : mode == 2 ? WP_CTRL : mode == 3 ? WP_NVM : WP_ALL);
    } else {
      usage_wp();
      return 2;
    }

    if (pmbus_wr_byte(fd, PMBUS_WRITE_PROTECT, vset) < 0) {
      perror("WRITE_PROTECT write");
      return 1;
    }
    int rb = pmbus_rd_byte(fd, PMBUS_WRITE_PROTECT);
    if (rb < 0) {
      perror("WRITE_PROTECT readback");
      return 1;
    }

    json_t *o = json_object();
    json_object_set_new(o, "changed", json_true());
    json_object_set_new(o, "WRITE_PROTECT", json_integer(rb));
    json_object_set_new(o, "none", json_boolean(rb == WP_NONE));
    json_object_set_new(o, "ctrl", json_boolean(rb == WP_CTRL));
    json_object_set_new(o, "nvm", json_boolean(rb == WP_NVM));
    json_object_set_new(o, "all", json_boolean(rb == WP_ALL));

    json_print_or_pretty(o, pretty);

    return 0;
  }

  usage_wp();

  return 2;
}
