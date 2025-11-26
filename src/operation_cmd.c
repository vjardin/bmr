/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "operation_cmd.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/*
 * PMBus OPERATION (0x01) standard fields:
 * bit7 : ON(1)/OFF(0)
 * bits6:5 : margin select: 00=normal, 01=margin_low, 10=margin_high, 11=reserved
 * other bits: device-specific (left unchanged when we compose)
 */

static void
decode_operation(uint8_t v, json_t *o) {
  json_object_set_new(o, "raw", json_integer(v));
  json_object_set_new(o, "on", json_boolean((v & 0x80) ? 1 : 0));

  const char *margin = "normal";
  switch ((v >> 5) & 0x3) {
  case 1:
    margin = "low";
    break;
  case 2:
    margin = "high";
    break;
  case 3:
    margin = "reserved";
    break;
  default:
    break;
  }

  json_object_set_new(o, "margin", json_string(margin));
}

static void
usage_operation(void) {
  fprintf(stderr,
"operation get\n"
"operation set [--on|--off] [--margin normal|low|high] [--raw 0xHH]\n"
  );
}

static int
parse_raw_byte(const char *s, uint8_t *out) {
  if (!s)
    return -1;

  char *end = NULL;
  errno = 0;
  long v = strtol(s, &end, 0);
  if (errno || end == s || *end != '\0' || v < 0 || v > 0xFF)
    return -1;

  *out = (uint8_t) v;

  return 0;
}

int
cmd_operation(int fd, int argc, char *const *argv, int pretty) {
  if (argc < 1) {
    usage_operation();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int val = pmbus_rd_byte(fd, PMBUS_OPERATION);
    if (val < 0) {
      perror("OPERATION");
      return 1;
    }
    json_t *o = json_object();
    decode_operation((uint8_t) val, o);
    json_print_or_pretty(o, pretty);
    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    int want_on = -1;           /* -1 keep, 0 off, 1 on */
    const char *margin = NULL;  /* normal|low|high */
    const char *raw = NULL;     /* 0xHH */

    for (int i = 1; i < argc; i++) {
      const char *a = argv[i];
      if (!strcmp(a, "--on"))
        want_on = 1;
      else if (!strcmp(a, "--off"))
        want_on = 0;
      else if (!strcmp(a, "--margin") && i + 1 < argc)
        margin = argv[++i];
      else if (!strcmp(a, "--raw") && i + 1 < argc)
        raw = argv[++i];
      else {
        usage_operation();
        return 2;
      }
    }

    json_t *delta = json_object();

    /* Raw write takes precedence */
    if (raw) {
      uint8_t b;
      if (parse_raw_byte(raw, &b) != 0) {
        fprintf(stderr, "--raw expects 0..255 (e.g. 0x80)\n");
        return 2;
      }
      if (pmbus_wr_byte(fd, PMBUS_OPERATION, b) < 0) {
        perror("OPERATION");
        return 1;
      }
      json_object_set_new(delta, "raw", json_integer(b));
    } else {
      /* read current, modify fields, write back */
      int cur = pmbus_rd_byte(fd, PMBUS_OPERATION);
      if (cur < 0) {
        perror("OPERATION");
        return 1;
      }
      uint8_t v = (uint8_t) cur;

      if (want_on == 0)
        v &= (uint8_t) ~ 0x80;
      else if (want_on == 1)
        v |= 0x80;

      if (margin) {
        /* clear bits 6:5 then set per choice */
        v &= (uint8_t) ~ 0x60;
        if (!strcmp(margin, "normal")) {
          /* 00 */
        } else if (!strcmp(margin, "low")) {
          v |= 0x20;            /* 01 */
        } else if (!strcmp(margin, "high")) {
          v |= 0x40;            /* 10 */
        } else {
          fprintf(stderr, "--margin: normal|low|high\n");
          return 2;
        }
      }

      if (pmbus_wr_byte(fd, PMBUS_OPERATION, v) < 0) {
        perror("OPERATION");
        return 1;
      }
      json_object_set_new(delta, "raw", json_integer(v));
      json_object_set_new(delta, "on", json_boolean((v & 0x80) ? 1 : 0));
      const char *m = "normal";
      switch ((v >> 5) & 0x3) {
      case 1:
        m = "low";
        break;
      case 2:
        m = "high";
        break;
      case 3:
        m = "reserved";
        break;
      default:
        fprintf(stderr, "invalid pmbus operation\n");
	return 2;
      }

      json_object_set_new(delta, "margin", json_string(m));
    }

    /* readback */
    int rb = pmbus_rd_byte(fd, PMBUS_OPERATION);
    json_t *out = json_object();
    json_object_set_new(out, "changed", delta);

    if (rb >= 0) {
      json_t *jrb = json_object();
      decode_operation((uint8_t) rb, jrb);
      json_object_set_new(out, "readback", jrb);
    }

    json_print_or_pretty(out, pretty);

    return 0;
  }

  usage_operation();

  return 2;
}
