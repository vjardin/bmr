/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "interleave_cmd.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void
usage_interleave(void) {
  fprintf(stderr,
"interleave get\n"
"interleave set [--set 0xNN] [--phases <1..16> --index <0..15>]\n"
  );
}

int
cmd_interleave(int fd, int argc, char *const *argv, int pretty) {
  if (argc == 0) {
    usage_interleave();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int rb = pmbus_rd_byte(fd, PMBUS_INTERLEAVE);
    if (rb < 0) {
      perror("INTERLEAVE");
      return 1;
    }

    int phases_m1 = (rb >> 4) & 0x0F;
    int idx = rb & 0x0F;

    json_t *o = json_object();
    json_object_set_new(o, "raw", json_integer(rb));
    json_object_set_new(o, "phases", json_integer(phases_m1 + 1));
    json_object_set_new(o, "phase_index", json_integer(idx));

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    const char *raw = NULL;
    int have_phases = 0, have_idx = 0;
    int phases = 0, idx = 0;

    for (int i = 1; i < argc; i++) {
      const char *a = argv[i];
      if (!strcmp(a, "--set") && i + 1 < argc)
        raw = argv[++i];
      else if (!strcmp(a, "--phases") && i + 1 < argc) {
        phases = atoi(argv[++i]);
        have_phases = 1;
      } else if (!strcmp(a, "--index") && i + 1 < argc) {
        idx = atoi(argv[++i]);
        have_idx = 1;
      }
    }

    uint8_t val = 0;
    if (raw) {
      char *end = NULL;
      errno = 0;
      long v = strtol(raw, &end, 0);
      if (errno || !end || *end) {
        usage_interleave();
        return 2;
      }
      if (v < 0 || v > 0xFF) {
        fprintf(stderr, "--set 0..255\n");
        return 2;
      }
      val = (uint8_t) v;
    } else if (have_phases && have_idx) {
      if (phases < 1 || phases > 16 || idx < 0 || idx > 15) {
        fprintf(stderr, "--phases 1..16, --index 0..15\n");
        return 2;
      }
      val = (uint8_t) (((phases - 1) & 0x0F) << 4) | (uint8_t) (idx & 0x0F);
    } else {
      usage_interleave();
      return 2;
    }

    if (pmbus_wr_byte(fd, PMBUS_INTERLEAVE, val) < 0) {
      perror("INTERLEAVE write");
      return 1;
    }
    int rb = pmbus_rd_byte(fd, PMBUS_INTERLEAVE);
    if (rb < 0) {
      perror("INTERLEAVE readback");
      return 1;
    }

    json_t *after = json_object();
    json_object_set_new(after, "raw", json_integer(rb));
    json_object_set_new(after, "phases", json_integer(((rb >> 4) & 0x0F) + 1));
    json_object_set_new(after, "phase_index", json_integer(rb & 0x0F));

    json_t *out = json_object();
    json_object_set_new(out, "changed", json_true());
    json_object_set_new(out, "readback", after);

    json_print_or_pretty(out, pretty);

    return 0;
  }

  usage_interleave();

  return 2;
}
