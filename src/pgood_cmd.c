/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmbus_io.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

static void
usage_pgood(void) {
  fprintf(stderr,
"pgood get [--exp5 <N>] [--raw]\n"
"pgood set [--on <V>] [--off <V>] [--exp5 <N>]  |  [--on-raw 0xNNNN] [--off-raw 0xNNNN]\n"
  );
}

int
cmd_pgood(int fd, int argc, char *const *argv, int pretty) {
  if (argc == 0) {
    usage_pgood();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int raw = 0, exp5 = 0, have_exp = 0;
    for (int i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--raw"))
        raw = 1;
      else if (!strcmp(argv[i], "--exp5") && i + 1 < argc) {
        exp5 = atoi(argv[++i]);
        have_exp = 1;
      }
    }
    if (!have_exp) {
      /* Auto-discover exp5 from VOUT_MODE if not provided */
      if (pmbus_get_vout_mode_exp(fd, &exp5) == 0)
        have_exp = 1;
    }
    int won = pmbus_rd_word(fd, PMBUS_POWER_GOOD_ON);
    int wof = pmbus_rd_word(fd, PMBUS_POWER_GOOD_OFF);
    if (won < 0 || wof < 0) {
      perror("PGOOD_*");
      return 1;
    }

    json_t *o = json_object();
    json_object_set_new(o, "PGOOD_ON_raw", json_integer((uint16_t) won));
    json_object_set_new(o, "PGOOD_OFF_raw", json_integer((uint16_t) wof));
    if (!raw && have_exp) {
      json_object_set_new(o, "PGOOD_ON_V", json_real(pmbus_lin16u_to_double((uint16_t) won, exp5)));
      json_object_set_new(o, "PGOOD_OFF_V", json_real(pmbus_lin16u_to_double((uint16_t) wof, exp5)));
      json_object_set_new(o, "exp5", json_integer(exp5));
    }

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    const char *on_raw = NULL, *off_raw = NULL, *on_v = NULL, *off_v = NULL;
    int have_exp = 0, exp5 = 0;
    for (int i = 1; i < argc; i++) {
      const char *a = argv[i];
      if (!strcmp(a, "--on-raw") && i + 1 < argc)
        on_raw = argv[++i];
      else if (!strcmp(a, "--off-raw") && i + 1 < argc)
        off_raw = argv[++i];
      else if (!strcmp(a, "--on") && i + 1 < argc)
        on_v = argv[++i];
      else if (!strcmp(a, "--off") && i + 1 < argc)
        off_v = argv[++i];
      else if (!strcmp(a, "--exp5") && i + 1 < argc) {
        exp5 = atoi(argv[++i]);
        have_exp = 1;
      }
    }

    uint16_t won = 0, wof = 0;
    int set_on = 0, set_off = 0;
    if (on_raw) {
      if (parse_u16(on_raw, &won)) {
        usage_pgood();
        return 2;
      }
      set_on = 1;
    }
    if (off_raw) {
      if (parse_u16(off_raw, &wof)) {
        usage_pgood();
        return 2;
      }
      set_off = 1;
    }

    if (on_v) {
      if (!have_exp && pmbus_get_vout_mode_exp(fd, &exp5) == 0)
        have_exp = 1;
      if (!have_exp) {
        fprintf(stderr, "--exp5 required with --on <V> (VOUT_MODE read failed)\n");
        return 2;
      }
      double v = strtod(on_v, NULL);
      /* N = v * 2^(−exp5) */
      double scaled = ldexp(v, -exp5);
      if (scaled < 0.0)
        scaled = 0.0;
      if (scaled > 65535.0)
        scaled = 65535.0;
      won = (uint16_t) (scaled + 0.5);

      set_on = 1;
    }
    if (off_v) {
      if (!have_exp && pmbus_get_vout_mode_exp(fd, &exp5) == 0)
        have_exp = 1;
      if (!have_exp) {
        fprintf(stderr, "--exp5 required with --off <V> (VOUT_MODE read failed)\n");
        return 2;
      }
      double v = strtod(off_v, NULL);
      /* N = v * 2^(−exp5) */
      double scaled = ldexp(v, -exp5);
      if (scaled < 0.0)
        scaled = 0.0;
      if (scaled > 65535.0)
        scaled = 65535.0;
      wof = (uint16_t) (scaled + 0.5);

      set_off = 1;
    }

    json_t *delta = json_object();
    if (set_on) {
      if (pmbus_wr_word(fd, PMBUS_POWER_GOOD_ON, won) < 0) {
        perror("PGOOD_ON write");
        return 1;
      }
      json_object_set_new(delta, "PGOOD_ON_raw", json_integer(won));
    }
    if (set_off) {
      if (pmbus_wr_word(fd, PMBUS_POWER_GOOD_OFF, wof) < 0) {
        perror("PGOOD_OFF write");
        return 1;
      }
      json_object_set_new(delta, "PGOOD_OFF_raw", json_integer(wof));
    }

    json_t *after = json_object();
    int rb_on = pmbus_rd_word(fd, PMBUS_POWER_GOOD_ON);
    int rb_of = pmbus_rd_word(fd, PMBUS_POWER_GOOD_OFF);
    if (rb_on >= 0)
      json_object_set_new(after, "PGOOD_ON_raw", json_integer((uint16_t) rb_on));
    if (rb_of >= 0)
      json_object_set_new(after, "PGOOD_OFF_raw", json_integer((uint16_t) rb_of));

    json_t *out = json_object();
    json_object_set_new(out, "changed", delta);
    json_object_set_new(out, "readback", after);
    if (have_exp)
      json_object_set_new(out, "exp5", json_integer(exp5));

    json_print_or_pretty(out, pretty);

    return 0;
  }

  usage_pgood();

  return 2;
}
