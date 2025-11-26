/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "onoff_cmd.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* BMR685 ON_OFF_CONFIG(0x02) bit mapping (datasheet):
 * bit4: Power-up policy         0=always on when input present, 1=controlled by sources
 * bit3: Use PMBus OPERATION     0=ignore OPERATION on/off,      1=use OPERATION on/off
 * bit2: Use CONTROL/EN pin      0=ignore pin,                   1=use pin
 * bit1: Pin polarity            0=active-low,                   1=active-high
 * bit0: Disable action          0=soft-stop (use TOFF_*),       1=immediate off
*/

static void
decode_onoff(uint8_t b, json_t *o) {
  json_object_set_new(o, "raw", json_integer(b));
  json_object_set_new(o, "powerup", json_string((b & 0x10) ? "controlled" : "always"));
  json_object_set_new(o, "use_operation", json_boolean((b & 0x08) ? 1 : 0));
  json_object_set_new(o, "use_pin", json_boolean((b & 0x04) ? 1 : 0));
  json_object_set_new(o, "pin_polarity", json_string((b & 0x02) ? "active_high" : "active_low"));
  json_object_set_new(o, "off_behavior", json_string((b & 0x01) ? "immediate" : "soft"));
}

static void
usage_onoff(void) {
  fprintf(stderr,
"onoff get\n"
"onoff set [--powerup always|controlled]\n"
"          [--source none|operation|pin|both]\n"
"          [--en-active high|low]\n"
"          [--off soft|immediate]\n"
"          [--raw 0xHH]\n"
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
cmd_onoff(int fd, int argc, char *const *argv, int pretty) {
  if (argc < 1) {
    usage_onoff();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int v = pmbus_rd_byte(fd, PMBUS_ON_OFF_CONFIG);
    if (v < 0) {
      perror("ON_OFF_CONFIG");
      return 1;
    }

    json_t *o = json_object();
    decode_onoff((uint8_t) v, o);

    /* also show OPERATION for convenience */
    int op = pmbus_rd_byte(fd, PMBUS_OPERATION);
    if (op >= 0) {
      json_t *jop = json_object();
      json_object_set_new(jop, "raw", json_integer(op));
      /* PMBus OPERATION (typical): bit7=on/off (1=on), bits[6:5] margin; rest impl-defined */
      json_object_set_new(jop, "on", json_boolean(((uint8_t) op & 0x80) ? 1 : 0));
      json_object_set_new(o, "OPERATION", jop);
    }

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    const char *powerup = NULL; /* always|controlled */
    const char *source = NULL;  /* none|operation|pin|both */
    const char *en_active = NULL; /* high|low */
    const char *off = NULL;     /* soft|immediate */
    const char *raw = NULL;     /* 0xHH */

    for (int i = 1; i < argc; i++) {
      const char *a = argv[i];
      if (!strcmp(a, "--powerup") && i + 1 < argc)
        powerup = argv[++i];
      else if (!strcmp(a, "--source") && i + 1 < argc)
        source = argv[++i];
      else if (!strcmp(a, "--en-active") && i + 1 < argc)
        en_active = argv[++i];
      else if (!strcmp(a, "--off") && i + 1 < argc)
        off = argv[++i];
      else if (!strcmp(a, "--raw") && i + 1 < argc)
        raw = argv[++i];
      else {
        usage_onoff();
        return 2;
      }
    }

    json_t *delta = json_object();

    /* If --raw is provided, write it as-is and show readback. */
    if (raw) {
      uint8_t b;
      if (parse_raw_byte(raw, &b) != 0) {
        fprintf(stderr, "--raw expects 0..255 (e.g. 0x1B)\n");
        return 2;
      }
      if (pmbus_wr_byte(fd, PMBUS_ON_OFF_CONFIG, b) < 0) {
        perror("ON_OFF_CONFIG");
        return 1;
      }
      json_object_set_new(delta, "raw", json_integer(b));
    } else {
      /* start from current value, then apply changes */
      int cur = pmbus_rd_byte(fd, PMBUS_ON_OFF_CONFIG);
      if (cur < 0) {
        perror("ON_OFF_CONFIG");
        return 1;
      }
      uint8_t b = (uint8_t) cur;

      if (powerup) {
        if (!strcmp(powerup, "always"))
          b &= (uint8_t) ~ 0x10;
        else if (!strcmp(powerup, "controlled"))
          b |= 0x10;
        else {
          fprintf(stderr, "--powerup: always|controlled\n");
          return 2;
        }
      }

      if (source) {
        if (!strcmp(source, "none")) {
          b &= (uint8_t) ~ 0x0C;  /* clear bits3..2 */
        } else if (!strcmp(source, "operation")) {
          b = (uint8_t) ((b & ~0x0C) | 0x08);
        } else if (!strcmp(source, "pin")) {
          b = (uint8_t) ((b & ~0x0C) | 0x04);
        } else if (!strcmp(source, "both")) {
          b |= 0x0C;
        } else {
          fprintf(stderr, "--source: none|operation|pin|both\n");
          return 2;
        }
      }

      if (en_active) {
        if (!strcmp(en_active, "low"))
          b &= (uint8_t) ~ 0x02;
        else if (!strcmp(en_active, "high"))
          b |= 0x02;
        else {
          fprintf(stderr, "--en-active: high|low\n");
          return 2;
        }
      }

      if (off) {
        if (!strcmp(off, "soft"))
          b &= (uint8_t) ~ 0x01;
        else if (!strcmp(off, "immediate"))
          b |= 0x01;
        else {
          fprintf(stderr, "--off: soft|immediate\n");
          return 2;
        }
      }

      if (pmbus_wr_byte(fd, PMBUS_ON_OFF_CONFIG, b) < 0) {
        perror("ON_OFF_CONFIG");
        return 1;
      }
      json_object_set_new(delta, "raw", json_integer(b));
      /* also echo decoded fields we intended to change */
      json_object_set_new(delta, "powerup", json_string((b & 0x10) ? "controlled" : "always"));
      json_object_set_new(delta, "source",
                          json_string((!(b & 0x0C)) ? "none" :
                                      ((b & 0x0C) == 0x08 ? "operation" : ((b & 0x0C) == 0x04 ? "pin" : "both"))));
      json_object_set_new(delta, "en_active", json_string((b & 0x02) ? "high" : "low"));
      json_object_set_new(delta, "off", json_string((b & 0x01) ? "immediate" : "soft"));
    }

    /* readback */
    int rb = pmbus_rd_byte(fd, PMBUS_ON_OFF_CONFIG);
    json_t *out = json_object();
    json_object_set_new(out, "changed", delta);
    if (rb >= 0) {
      json_t *jrb = json_object();
      decode_onoff((uint8_t) rb, jrb);
      json_object_set_new(out, "readback", jrb);
    }
    json_print_or_pretty(out, pretty);

    return 0;
  }

  usage_onoff();

  return 2;
}
