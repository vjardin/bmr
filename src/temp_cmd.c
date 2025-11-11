/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "util_json.h"

#include <jansson.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Linear11: 5-bit signed exponent (E), 11-bit signed mantissa (Y), value = Y * 2^E
 *   raw[15:11] = E (two's complement), raw[10:0] = Y (two's complement)
 */

static inline long
lround_compat(double d) {
  return (long) ((d >= 0.0) ? (d + 0.5) : (d - 0.5));
}

static inline int32_t
sign_extend(int32_t v, unsigned bits) {
  int32_t m = 1u << (bits - 1);

  v = v & ((1u << bits) - 1);
  return (v ^ m) - m;
}

static double
lin11_to_double(uint16_t raw) {
  int8_t E = (int8_t) sign_extend((raw >> 11) & 0x1F, 5);
  int16_t Y = (int16_t) sign_extend(raw & 0x7FF, 11);

  double val = (double) Y;
  if (E >= 0) {
    val *= (double) (1u << E);
  } else {
    val /= (double) (1u << (-E));
  }

  return val;
}

static uint16_t
double_to_lin11(double v) {
  if (v == 0.0)
    return 0;

  /* Choose E in [-16..15] so Y fits in [-1024..1023] with best resolution. */
  int bestE = 0;
  long bestY = 0;
  long bestAbsY = -1;

  for (int E = -16; E <= 15; ++E) {
    double scaled = (E >= 0) ? (v / (double) (1u << E)) : (v * (double) (1u << (-E)));
    long Y = lround_compat(scaled);
    if (Y < -1024 || Y > 1023)
      continue;
    long a = (Y >= 0) ? Y : -Y;
    if (a > bestAbsY) {
      bestAbsY = a;
      bestE = E;
      bestY = Y;
      if (bestAbsY == 1023)
        break;
    }
  }

  /* If nothing fit (huge magnitude), clamp using the nearest legal E. */
  if (bestAbsY < 0) {
    /* pick E to bring into range */
    int E = 0;
    long Y = 0;
    for (E = 15; E >= -16; --E) {
      double scaled = (E >= 0) ? (v / (double) (1u << E)) : (v * (double) (1u << (-E)));
      Y = lround_compat(scaled);
      if (Y >= -1024 && Y <= 1023)
        break;
    }
    if (Y < -1024)
      Y = -1024;
    if (Y > 1023)
      Y = 1023;
    bestE = E;
    bestY = Y;
  }

  uint16_t Ebits = (uint16_t) (bestE & 0x1F);
  uint16_t Ybits = (uint16_t) (bestY & 0x7FF);

  return (uint16_t) ((Ebits << 11) | Ybits);
}

/* Parse temperatures: accepts:  "85", "85C", "-40C", "358K", "185F" (C default) */
static int
parse_temp_celsius(const char *s, double *outC) {
  if (!s || !*s)
    return -1;

  /* trim spaces */
  while (isspace((unsigned char) *s))
    s++;

  char *end = NULL;
  errno = 0;
  double v = strtod(s, &end);
  if (errno != 0 || end == s)
    return -1;

  /* skip spaces */
  while (end && isspace((unsigned char) *end))
    end++;

  double C = v;
  if (end && *end) {
    char u = (char) toupper((unsigned char) *end);
    if (u == 'C') {
      C = v;
    } else if (u == 'K') {
      C = v - 273.15;
    } else if (u == 'F') {
      C = (v - 32.0) * (5.0 / 9.0);
    } else {
      /* unknown */
      return -1;
    }
    end++;
    while (end && isspace((unsigned char) *end))
      end++;
    if (end && *end) /* junks */
      return -1;
  }

  *outC = C;

  return 0;
}

static void
put_temp_limit(json_t *dst, const char *key, int fd, uint8_t cmd) {
  int w = pmbus_rd_word(fd, cmd);
  json_t *o = json_object();

  if (w >= 0) {
    uint16_t raw = (uint16_t) w;
    double C = lin11_to_double(raw);
    int8_t E = (int8_t) sign_extend((raw >> 11) & 0x1F, 5);
    int16_t Y = (int16_t) sign_extend(raw & 0x7FF, 11);

    json_object_set_new(o, "raw", json_integer(raw));
    json_object_set_new(o, "C", json_real(C));
    json_object_set_new(o, "lin11_exp", json_integer(E));
    json_object_set_new(o, "lin11_man", json_integer(Y));
  } else {
    json_object_set_new(o, "error", json_integer(w));
  }

  json_object_set_new(dst, key, o);
}

static void
put_temp_read(json_t *dst, const char *key, int fd, uint8_t cmd) {
  int w = pmbus_rd_word(fd, cmd);
  json_t *o = json_object();

  if (w >= 0) {
    uint16_t raw = (uint16_t) w;
    double C = lin11_to_double(raw);
    int8_t E = (int8_t) sign_extend((raw >> 11) & 0x1F, 5);
    int16_t Y = (int16_t) sign_extend(raw & 0x7FF, 11);

    json_object_set_new(o, "raw", json_integer(raw));
    json_object_set_new(o, "C", json_real(C));
    json_object_set_new(o, "lin11_exp", json_integer(E));
    json_object_set_new(o, "lin11_man", json_integer(Y));
  } else {
    json_object_set_new(o, "error", json_integer(w));
  }

  json_object_set_new(dst, key, o);
}

static void
usage_temp_short(void) {
  fprintf(stderr,
"Usage:\n"
"  temp get [all|ot|ut|warn]\n"
"  temp set [--ot-fault <C>] [--ut-fault <C>] [--ot-warn <C>] [--ut-warn <C>]\n"
"  temp read [all|t1|t2|t3]\n"
"  temp help\n"
"\n"
"Notes: values are Celsius by default; suffix K or F is accepted (e.g., 358K, 185F, have fun).\n"
  );
}

static void
usage_temp_long(void) {
  fprintf(stderr,
"bmr temp — read/set temperature limits and live temperatures (PMBus Linear11)\n"
"\n"
"Linear11 format:\n"
"  value = mantissa * 2^exponent, with exponent (5-bit signed) and mantissa (11-bit signed).\n"
"  This tool converts to/from °C and also prints the raw word and decoded fields.\n"
"\n"
"Commands:\n"
"  temp get  [all|ot|ut|warn]\n"
"      Read OT/UT FAULT and WARN limits. Output JSON shows raw Linear11 and decoded °C.\n"
"\n"
"  temp set  [--ot-fault <C>] [--ut-fault <C>] [--ot-warn <C>] [--ut-warn <C>]\n"
"      Write one or more limits. Units accepted: C (default), K, F.\n"
"      Examples: 110   (110°C), 110C, 358K (~85°C), 185F (~85°C).\n"
"\n"
"  temp read [all|t1|t2|t3]\n"
"      Read live temperature sensors (READ_TEMPERATURE_1/2/(3 if present)).\n"
"      Keys: READ_TEMPERATURE_1, READ_TEMPERATURE_2, READ_TEMPERATURE_3.\n"
"\n"
"Good practice:\n"
"  * After changing limits that must persist, run:  user-data set --store  (and optionally restart).\n"
"  * Pair with 'fault temp set' to create timed OFF → auto-retry sequences (e.g., 16 s using 2^n delays).\n"
"\n"
"Examples:\n"
"  bmr --bus /dev/i2c-220 --addr 0x15 temp get all\n"
"  bmr --bus /dev/i2c-220 --addr 0x15 temp set --ot-fault 110 --ut-fault -40\n"
"  bmr --bus /dev/i2c-220 --addr 0x15 temp read all\n"
  );
}

static int
write_one_limit(int fd, const char *label, uint8_t cmd, const char *val_s, json_t *w, json_t *rb) {
  double C = 0.0;
  if (parse_temp_celsius(val_s, &C) < 0) {
    fprintf(stderr, "bad value for %s\n", label);
    return 2;
  }
  uint16_t raw = double_to_lin11(C);
  int rc = pmbus_wr_word(fd, cmd, raw);

  json_t *wo = json_object();
  json_object_set_new(wo, "C", json_real(C));
  json_object_set_new(wo, "raw", json_integer(raw));
  json_object_set_new(w, label, wo);

  int rbw = pmbus_rd_word(fd, cmd);
  if (rbw >= 0) {
    uint16_t r = (uint16_t) rbw;
    double C2 = lin11_to_double(r);
    json_t *rbo = json_object();
    json_object_set_new(rbo, "C", json_real(C2));
    json_object_set_new(rbo, "raw", json_integer(r));
    json_object_set_new(rb, label, rbo);
  }

  return (rc < 0) ? 1 : 0;
}

int
cmd_temp(int fd, int argc, char *const *argv, int pretty) {
  if (argc < 1) {
    usage_temp_short();
    return 2;
  }

  const char *sub = argv[0];

  if (!strcmp(sub, "help") || !strcmp(sub, "--help") || !strcmp(sub, "-h")) {
    usage_temp_long();
    return 0;
  }

  if (!strcmp(sub, "get")) {
    const char *which = (argc > 1) ? argv[1] : "all";
    json_t *root = json_object();

    if (!strcmp(which, "all") || !strcmp(which, "ot")) {
      json_t *ot = json_object();
      put_temp_limit(ot, "OT_FAULT_LIMIT", fd, PMBUS_OT_FAULT_LIMIT);
      put_temp_limit(ot, "OT_WARN_LIMIT", fd, PMBUS_OT_WARN_LIMIT);
      json_object_set_new(root, "ot", ot);
    }

    if (!strcmp(which, "all") || !strcmp(which, "ut")) {
      json_t *ut = json_object();
      put_temp_limit(ut, "UT_WARN_LIMIT", fd, PMBUS_UT_WARN_LIMIT);
      put_temp_limit(ut, "UT_FAULT_LIMIT", fd, PMBUS_UT_FAULT_LIMIT);
      json_object_set_new(root, "ut", ut);
    }

    if (!strcmp(which, "all") || !strcmp(which, "warn")) {
      json_t *warn = json_object();
      put_temp_limit(warn, "OT_WARN_LIMIT", fd, PMBUS_OT_WARN_LIMIT);
      put_temp_limit(warn, "UT_WARN_LIMIT", fd, PMBUS_UT_WARN_LIMIT);
      json_object_set_new(root, "warn", warn);
    }

    json_print_or_pretty(root, pretty);

    return 0;
  }

  if (!strcmp(sub, "set")) {
    const char *ot_fault_s = NULL, *ut_fault_s = NULL;
    const char *ot_warn_s = NULL, *ut_warn_s = NULL;

    for (int i = 1; i < argc; ++i) {
      if (!strcmp(argv[i], "--ot-fault") && i + 1 < argc) {
        ot_fault_s = argv[++i];
        continue;
      }
      if (!strcmp(argv[i], "--ut-fault") && i + 1 < argc) {
        ut_fault_s = argv[++i];
        continue;
      }
      if (!strcmp(argv[i], "--ot-warn") && i + 1 < argc) {
        ot_warn_s = argv[++i];
        continue;
      }
      if (!strcmp(argv[i], "--ut-warn") && i + 1 < argc) {
        ut_warn_s = argv[++i];
        continue;
      }
    }

    if (!ot_fault_s && !ut_fault_s && !ot_warn_s && !ut_warn_s) {
      usage_temp_short();
      return 2;
    }

    json_t *out = json_object();
    json_t *w = json_object();
    json_t *rb = json_object();
    int rc_or = 0;

    if (ot_fault_s) {
      int rc = write_one_limit(fd, "OT_FAULT_LIMIT", PMBUS_OT_FAULT_LIMIT, ot_fault_s, w, rb);
      if (rc == 2)
        rc_or = 2;
      else if (rc == 1 && rc_or == 0)
        rc_or = 1;
    }
    if (ut_fault_s) {
      int rc = write_one_limit(fd, "UT_FAULT_LIMIT", PMBUS_UT_FAULT_LIMIT, ut_fault_s, w, rb);
      if (rc == 2)
        rc_or = 2;
      else if (rc == 1 && rc_or == 0)
        rc_or = 1;
    }
    if (ot_warn_s) {
      int rc = write_one_limit(fd, "OT_WARN_LIMIT", PMBUS_OT_WARN_LIMIT, ot_warn_s, w, rb);
      if (rc == 2)
        rc_or = 2;
      else if (rc == 1 && rc_or == 0)
        rc_or = 1;
    }
    if (ut_warn_s) {
      int rc = write_one_limit(fd, "UT_WARN_LIMIT", PMBUS_UT_WARN_LIMIT, ut_warn_s, w, rb);
      if (rc == 2)
        rc_or = 2;
      else if (rc == 1 && rc_or == 0)
        rc_or = 1;
    }

    json_object_set_new(out, "wrote", w);
    json_object_set_new(out, "readback", rb);

    json_print_or_pretty(out, pretty);

    return rc_or;
  }

  if (!strcmp(sub, "read")) {
    const char *which = (argc > 1) ? argv[1] : "all";
    json_t *root = json_object();

    if (!strcmp(which, "all") || !strcmp(which, "t1")) {
      put_temp_read(root, "READ_TEMPERATURE_1", fd, PMBUS_READ_TEMPERATURE_1);
    }
    if (!strcmp(which, "all") || !strcmp(which, "t2")) {
      put_temp_read(root, "READ_TEMPERATURE_2", fd, PMBUS_READ_TEMPERATURE_2);
    }
    if (!strcmp(which, "all") || !strcmp(which, "t3")) {
      put_temp_read(root, "READ_TEMPERATURE_3", fd, PMBUS_READ_TEMPERATURE_3);
    }

    json_print_or_pretty(root, pretty);

    return 0;
  }

  usage_temp_short();
  return 2;
}
