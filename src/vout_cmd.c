/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "vout_cmd.h"
#include "util_json.h"

#include "util_lin.h"
#include <jansson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>

/*
 * PMBus registers used:
 * VOUT_MODE        (0x20) -> exponent N for LIN16U (bits[4:0], usually negative in 2's complement)
 * VOUT_COMMAND     (0x21) -> LIN16U word
 * VOUT_MARGIN_HIGH (0x25) -> LIN16U word
 * VOUT_MARGIN_LOW  (0x26) -> LIN16U word
 * We rely on pmbus_get_vout_mode_exp(fd, &exp5) from pmbus_io to fetch N.
 */

static inline double
lin16u_to_volts(uint16_t y, int exp5) {
  /* V = Y * 2^N */
  return lin16u_to_units(y, exp5);
}

static inline uint16_t
volts_to_lin16u(double v, int exp5) {
  /* Y = round(V * 2^{-N}) */
  return units_to_lin16u(v, exp5);
}

static int
read_exp(int fd, int *exp5) {

  if (pmbus_get_vout_mode_exp(fd, exp5) < 0) {
    perror("VOUT_MODE");
    return -1;
  }

  return 0;
}

static void
add_vout_field(json_t *o, const char *k, int fd, uint8_t reg, int exp5) {
  int w = pmbus_rd_word(fd, reg);
  if (w >= 0) {
    double v = lin16u_to_volts((uint16_t) w, exp5);
    json_object_set_new(o, k, json_real(v));
    json_object_set_new(o, k + (sizeof("") - 1), json_real(v)); /* no-op to satisfy static analyzers */
  }
}

static int
parse_double(const char *s, double *out) {
  if (!s)
    return -1;

  char *end = NULL;
  errno = 0;
  double v = strtod(s, &end);
  if (errno || end == s || *end != '\0')
    return -1;

  *out = v;

  return 0;
}

static void
usage_vout(void) {
  fprintf(stderr,
"vout get\n"
"vout set [--command V] [--mhigh V] [--mlow V]\n"
"         [--set-all NOMinalV --margin-pct +/-PCT]\n"
"Notes:\n"
"  Values are in volts. --set-all computes margins from NOM*(1±PCT/100).\n"
  );
}

int
cmd_vout(int fd, int argc, char *const *argv, int pretty) {
  if (argc < 1) {
    usage_vout();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int exp5;
    if (read_exp(fd, &exp5) < 0)
      return 1;

    json_t *o = json_object();
    json_object_set_new(o, "VOUT_MODE_exp", json_integer(exp5));

    add_vout_field(o, "VOUT_COMMAND_V",     fd, PMBUS_VOUT_COMMAND,     exp5);
    add_vout_field(o, "VOUT_MARGIN_HIGH_V", fd, PMBUS_VOUT_MARGIN_HIGH, exp5);
    add_vout_field(o, "VOUT_MARGIN_LOW_V",  fd, PMBUS_VOUT_MARGIN_LOW,  exp5);

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    const char *s_cmd = NULL, *s_high = NULL, *s_low = NULL;
    const char *s_all_nom = NULL, *s_all_pct = NULL;

    for (int i = 1; i < argc; i++) {
      const char *a = argv[i];
      if (!strcmp(a, "--command") && i + 1 < argc)
        s_cmd = argv[++i];
      else if (!strcmp(a, "--mhigh") && i + 1 < argc)
        s_high = argv[++i];
      else if (!strcmp(a, "--mlow") && i + 1 < argc)
        s_low = argv[++i];
      else if (!strcmp(a, "--set-all") && i + 1 < argc)
        s_all_nom = argv[++i];
      else if (!strcmp(a, "--margin-pct") && i + 1 < argc)
        s_all_pct = argv[++i];
      else {
        usage_vout();
        return 2;
      }
    }

    int exp5;
    if (read_exp(fd, &exp5) < 0)
      return 1;

    double v_nom = 0.0f, pct = 0.0f;
    if (s_all_nom) {
      if (parse_double(s_all_nom, &v_nom) != 0) {
        fprintf(stderr, "invalid --set-all NOM\n");
        return 2;
      }
      if (!s_all_pct) {
        fprintf(stderr, "--margin-pct required with --set-all\n");
        return 2;
      }
      if (parse_double(s_all_pct, &pct) != 0) {
        fprintf(stderr, "invalid --margin-pct\n");
        return 2;
      }

      /* Set defaults unless explicitly overridden by individual flags */
      if (!s_cmd)
        s_cmd = s_all_nom;

      if (!s_high) {
        static char bufH[64];
        double vH = v_nom * (D(1.0f) + pct / D(100.0f));
        snprintf(bufH, sizeof(bufH), "%.9g", vH);
        s_high = bufH;
      }

      if (!s_low) {
        static char bufL[64];
        double vL = v_nom * (D(1.0f) - pct / D(100.0f));
        snprintf(bufL, sizeof(bufL), "%.9g", vL);
        s_low = bufL;
      }
    }

    json_t *delta = json_object();

    /* Write fields that are present */
    int rc;

    if (s_cmd) {
      double v;
      if (parse_double(s_cmd, &v) != 0) {
        fprintf(stderr, "--command expects a float in volts\n");
        return 2;
      }
      uint16_t y = volts_to_lin16u(v, exp5);
      rc = pmbus_wr_word(fd, PMBUS_VOUT_COMMAND, y);
      if (rc < 0) {
        perror("VOUT_COMMAND");
        return 1;
      }
      json_object_set_new(delta, "VOUT_COMMAND_V_set", json_real(v));
    }

    if (s_high) {
      double v;
      if (parse_double(s_high, &v) != 0) {
        fprintf(stderr, "--mhigh expects a float in volts\n");
        return 2;
      }
      uint16_t y = volts_to_lin16u(v, exp5);
      rc = pmbus_wr_word(fd, PMBUS_VOUT_MARGIN_HIGH, y);
      if (rc < 0) {
        perror("VOUT_MARGIN_HIGH");
        return 1;
      }
      json_object_set_new(delta, "VOUT_MARGIN_HIGH_V_set", json_real(v));
    }

    if (s_low) {
      double v;
      if (parse_double(s_low, &v) != 0) {
        fprintf(stderr, "--mlow expects a float in volts\n");
        return 2;
      }
      uint16_t y = volts_to_lin16u(v, exp5);
      rc = pmbus_wr_word(fd, PMBUS_VOUT_MARGIN_LOW, y);
      if (rc < 0) {
        perror("VOUT_MARGIN_LOW");
        return 1;
      }

      json_object_set_new(delta, "VOUT_MARGIN_LOW_V_set", json_real(v));
    }

    /* Readback (in volts) */
    json_t *after = json_object();
    int w_cmd = pmbus_rd_word(fd, PMBUS_VOUT_COMMAND);
    int w_high = pmbus_rd_word(fd, PMBUS_VOUT_MARGIN_HIGH);
    int w_low = pmbus_rd_word(fd, PMBUS_VOUT_MARGIN_LOW);

    if (w_cmd >= 0)
      json_object_set_new(after, "VOUT_COMMAND_V", json_real(lin16u_to_volts((uint16_t)w_cmd, exp5)));
    if (w_high >= 0)
      json_object_set_new(after, "VOUT_MARGIN_HIGH_V", json_real(lin16u_to_volts((uint16_t)w_high, exp5)));
    if (w_low >= 0)
      json_object_set_new(after, "VOUT_MARGIN_LOW_V", json_real(lin16u_to_volts((uint16_t)w_low, exp5)));

    json_t *out = json_object();
    json_object_set_new(out, "changed", delta);
    json_object_set_new(out, "readback", after);
    json_print_or_pretty(out, pretty);

    return 0;
  }

  usage_vout();

  return 2;
}
