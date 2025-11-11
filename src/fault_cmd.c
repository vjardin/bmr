/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

/*
 * Helpers to decode the generic PMBus response byte (Table 4 in PMBus Part II)
 * for voltage/temperature/TonMax faults, and Flex AN302.
 *
 * Bits [7:6] Response:
 *   00 ignore
 *   01 delay-then-apply-retry-setting
 *   10 disable-then-apply-retry-setting
 *   11 disable-until-fault-clears
 * Bits [5:3] Retries: 0..6, 7=continuous
 * Bits [2:0] Delay time "count" in a unit that depends on the command:
 *   - OT/UT: seconds = 2^n (n in 0..7)
 *   - VIN/VOUT/TonMax: units are 10 ms/LSB (Flex BMR45x) unless otherwise documented.
 *
 * UT = Under-Temperature (temperature below the limit).
 * OT = Over-Temperature (temperature above the limit).
 * OV = Over-Voltage (voltage above the limit).
 * UV = Under-Voltage (voltage below the limit).
 * OC = Over-Current.
 * TON_MAX = “time to reach regulation exceeded.”
 */

#define MODE_IGNORE 0
#define MODE_DELAY_THEN_RETRY 1
#define MODE_DISABLE_AND_RETRY 2
#define MODE_DISABLE_UNTIL_CLEAR 3

static const char *
resp_mode_name(uint8_t b) {
  switch ((b >> 6) & 0x3) {
    case MODE_IGNORE:
      return "ignore";
    case MODE_DELAY_THEN_RETRY:
      return "delay-then-retry";
    case MODE_DISABLE_AND_RETRY:
      return "disable-and-retry";
    case MODE_DISABLE_UNTIL_CLEAR:
      return "disable-until-clear";
    }

  return "unknown";
}

static void
decode_retry(uint8_t b, json_t *o) {
  unsigned r = (b >> 3) & 0x7;
  if (r == 7)
    json_object_set_new(o, "retries", json_string("continuous"));
  else
    json_object_set_new(o, "retries", json_integer(r));
}

static inline unsigned
clamp_u(unsigned v, unsigned maxv) {
  return v > maxv ? maxv : v;
}

static void
decode_delay_temp(uint8_t b, json_t *o) {
  unsigned n = b & 0x7;
  unsigned secs = 1u << n;

  json_object_set_new(o, "delay_unit", json_string("2^n seconds"));
  json_object_set_new(o, "delay_n", json_integer(n));
  json_object_set_new(o, "delay_seconds", json_integer(secs));
}

static void
decode_delay_ms10(uint8_t b, json_t *o) {
  unsigned ticks = b & 0x7;
  unsigned ms = ticks * 10u;

  json_object_set_new(o, "delay_unit", json_string("10ms"));
  json_object_set_new(o, "delay_ticks", json_integer(ticks));
  json_object_set_new(o, "delay_ms", json_integer(ms));
}

static void
put_resp_byte(json_t *dst, const char *key, int fd, uint8_t cmd, bool is_temp_family) {
  int v = pmbus_rd_byte(fd, cmd);

  json_t *o = json_object();

  if (v >= 0) {
    uint8_t b = (uint8_t) v;
    json_object_set_new(o, "raw", json_integer(b));
    json_object_set_new(o, "mode", json_string(resp_mode_name(b)));
    decode_retry(b, o);
    if (is_temp_family)
      decode_delay_temp(b, o);
    else
      decode_delay_ms10(b, o);

  } else {
    json_object_set_new(o, "error", json_integer(v));
  }

  json_object_set_new(dst, key, o);
}

static void
usage_fault(void) {
  fprintf(stderr,
"fault get [all|temp|vin|vout|tonmax|iout]\n"
"fault temp set [--ot-delay 16s|32s|2^n] [--ot-mode ignore|delay-retry|disable-retry|disable-until-clear]\n"
"               [--ot-retries 0..6|cont]\n"
"               [--ut-delay 16s|32s|2^n] [--ut-mode ignore|delay-retry|disable-retry|disable-until-clear]\n"
"               [--ut-retries 0..6|cont]\n"
"Hint: run 'fault help' for detailed documentation.\n"
"\n"
"Examples:\n"
"  # Disable output and retry continuously every 16s on OT and UT\n"
"  fault temp set --ot-delay 16s --ot-mode disable-retry --ot-retries cont \\\n"
"                 --ut-delay 16s --ut-mode disable-retry --ut-retries cont\n"
  );
}

static void
usage_fault_long(void) {
  fprintf(stderr,
"bmr fault — set/read PMBus FAULT RESPONSE bytes\n"
"\n"
"Abbreviations:\n"
"  OT=Over-Temperature, UT=Under-Temperature, OV=Over-Voltage, UV=Under-Voltage, OC=Over-Current.\n"
"\n"
"Response byte format:\n"
"  Bits[7:6] mode: 00 ignore | 01 delay-then-retry | 10 disable-and-retry | 11 disable-until-fault-clears (latchoff)\n"
"  Bits[5:3] retries: 0..6, 7=continuous\n"
"  Bits[2:0] delay field (unit depends on command family):\n"
"     * Temperature (OT/UT): seconds = 2^n, n in [0..7]  →  n=4→16s, n=5→32s\n"
"     * VIN/VOUT/TON_MAX/IOUT: typically 10 ms per LSB on BMR45x (see device spec)\n"
"\n"
"Mode details (what the converter does when a fault occurs):\n"
"  00  ignore\n"
"      * No protective action is taken on the output; the rail stays in its current state.\n"
"      * The fault is still *reported* via STATUS registers (and may assert SMBALERT# if unmasked).\n"
"      * 'retries' and 'delay' fields are ignored in this mode.\n"
"\n"
"  01  delay-then-retry\n"
"      * The converter *does not immediately shut down*; it waits the programmed 'delay'.\n"
"      * After the delay, a retry policy is applied:\n"
"          - If the fault condition has cleared, the device resumes normal operation.\n"
"          - If the condition persists, a restart/enable attempt may be made per 'retries'.\n"
"      * 'retries' = 0..6 limits the number of retry attempts; 7 = continuous.\n"
"      * Temperature family uses 2^n-second delays; other families use 10 ms/LSB delays.\n"
"\n"
"  10  disable-and-retry\n"
"      * The output is *disabled immediately* (soft or immediate off per device/policy).\n"
"      * After the programmed 'delay', the device attempts to re-enable.\n"
"      * Retries follow the 'retries' count (0..6) or continue indefinitely when set to 7 (continuous).\n"
"      * If retries are exhausted and the fault persists, the device remains disabled until explicitly re-enabled\n"
"        (e.g., OPERATION on / CTRL release / CLEAR_FAULTS / power-cycle / vendor restart).\n"
"\n"
"  11  disable-until-fault-clears  (\"latchoff\")\n"
"      * The output is disabled and *no automatic retries* are attempted.\n"
"      * The device stays off until BOTH: (a) the fault condition is no longer present, and (b) you re-enable it\n"
"        according to your on/off policy (OPERATION/CTRL) or power-cycle/restart. 'retries' and 'delay' are ignored.\n"
"\n"
"Commands:\n"
"  fault get [all|temp|vin|vout|tonmax|iout]\n"
"      Read and decode response byte(s) to JSON (mode/retries/delay with proper units).\n"
"\n"
"  fault temp set [options]\n"
"      Program OT/UT FAULT RESPONSE bytes.\n"
"      --ot-delay <16s|32s|2^n|n=N>   --ut-delay <...>\n"
"      --ot-mode <ignore|delay-retry|disable-retry|disable-until-clear|latchoff>\n"
"      --ut-mode <...same values...>\n"
"      --ot-retries <0..6|cont|continuous>   --ut-retries <0..6|cont|continuous>\n"
"\n"
"Accepted delay forms (temperature): \"16s\", \"32s\", \"2^4\", \"2^5\", \"n=4\", or just \"4\" (meaning 2^4 s).\n"
"\n"
"Notes:\n"
"  * Temperature family uses exponential seconds (2^n). VIN/VOUT/TON_MAX/IOUT use 10 ms/LSB on BMR45x.\n"
"  * After changing behavior that must persist, run:  user-data set --store  (and optionally restart).\n"
"  * In parallel systems configure each module individually; uncontrolled hiccup is discouraged.\n"
"\n"
"Examples:\n"
"  bmr --bus /dev/i2c-220 --addr 0x15 fault get all\n"
"  bmr --bus /dev/i2c-220 --addr 0x15 fault temp set \\\n"
"     --ot-delay 16s --ot-mode disable-retry --ot-retries cont \\\n"
"     --ut-delay 16s --ut-mode disable-retry --ut-retries cont\n"
  );
}

static int
parse_n_from_arg(const char *s) {
  if (!s)
    return -1;

  /* Accept forms: "16s", "32s", "2^4", "2^5", "n=4", "4" */
  if (strchr(s, 's')) {
    long v = strtol(s, NULL, 10);
    if (v <= 0)
      return -1;
    /* find n such that 2^n == v */
    unsigned n = 0;
    unsigned val = 1;
    while (n < 7 && val < (unsigned) v) {
      n++;
      val <<= 1;
    }
    if (val != (unsigned) v)
      return -1;

    return (int)n;
  }

  if (!strncmp(s, "2^", 2)) {
    long n = strtol(s + 2, NULL, 10);
    if (n < 0 || n > 7)
      return -1;

    return (int)n;
  }

  if (!strncmp(s, "n=", 2)) {
    long n = strtol(s + 2, NULL, 10);
    if (n < 0 || n > 7)
      return -1;

    return (int)n;
  }

  char *end = NULL;
  errno = 0;
  long n = strtol(s, &end, 0);

  if (!errno && end && *end == '\0' && n >= 0 && n <= 7)
    return (int)n;

  return -1;
}

static uint8_t
compose_resp_byte(uint8_t mode, int retries, int delay_n) {
  uint8_t b = 0;

  b |= (uint8_t) ((mode & 0x3) << 6);

  if (retries < 0)
    retries = 7;

  b |= (uint8_t) ((clamp_u((unsigned) retries, 7) & 0x7) << 3);
  b |= (uint8_t) (clamp_u((unsigned) delay_n, 7) & 0x7);

  return b;
}

static int
parse_mode(const char *s, uint8_t *mode_out) {
  if (!s)
    return -1;

  if (!strcmp(s, "ignore")) {
    *mode_out = MODE_IGNORE;
    return 0;
  }

  if (!strcmp(s, "delay-retry") || !strcmp(s, "delay-then-retry")) {
    *mode_out = MODE_DELAY_THEN_RETRY;
    return 0;
  }

  if (!strcmp(s, "disable-retry") || !strcmp(s, "disable-and-retry")) {
    *mode_out = MODE_DISABLE_AND_RETRY;
    return 0;
  }

  if (!strcmp(s, "disable-until-clear") || !strcmp(s, "latchoff")) {
    *mode_out = MODE_DISABLE_UNTIL_CLEAR;
    return 0;
  }

  return -1;
}

int
cmd_fault(int fd, int argc, char *const *argv, int pretty) {
  if (argc < 1) {
    usage_fault();
    return 2;
  }
  const char *sub = argv[0];

  if (!strcmp(sub, "help") || !strcmp(sub, "--help") || !strcmp(sub, "-h")) {
    usage_fault_long();
    return 0;
  }

  if (!strcmp(sub, "get")) {
    const char *which = (argc > 1) ? argv[1] : "all";
    json_t *root = json_object();

    if (!strcmp(which, "all") || !strcmp(which, "temp")) {
      json_t *temp = json_object();
      put_resp_byte(temp, "OT_FAULT_RESPONSE", fd, PMBUS_OT_FAULT_RESPONSE, true);
      put_resp_byte(temp, "UT_FAULT_RESPONSE", fd, PMBUS_UT_FAULT_RESPONSE, true);
      json_object_set_new(root, "temperature", temp);
    }

    if (!strcmp(which, "all") || !strcmp(which, "vout")) {
      json_t *vout = json_object();
      put_resp_byte(vout, "VOUT_OV_FAULT_RESPONSE", fd, PMBUS_VOUT_OV_FAULT_RESPONSE, false);
      put_resp_byte(vout, "VOUT_UV_FAULT_RESPONSE", fd, PMBUS_VOUT_UV_FAULT_RESPONSE, false);
      json_object_set_new(root, "vout", vout);
    }

    if (!strcmp(which, "all") || !strcmp(which, "vin")) {
      json_t *vin = json_object();
      put_resp_byte(vin, "VIN_OV_FAULT_RESPONSE", fd, PMBUS_VIN_OV_FAULT_RESPONSE, false);
      put_resp_byte(vin, "VIN_UV_FAULT_RESPONSE", fd, PMBUS_VIN_UV_FAULT_RESPONSE, false);
      json_object_set_new(root, "vin", vin);
    }

    if (!strcmp(which, "all") || !strcmp(which, "tonmax")) {
      json_t *tm = json_object();
      put_resp_byte(tm, "TON_MAX_FAULT_RESPONSE", fd, PMBUS_TON_MAX_FAULT_RESPONSE, false);
      json_object_set_new(root, "tonmax", tm);
    }

    if (!strcmp(which, "all") || !strcmp(which, "iout")) {
      json_t *io = json_object();
      put_resp_byte(io, "IOUT_OC_FAULT_RESPONSE", fd, PMBUS_IOUT_OC_FAULT_RESPONSE, false);
      json_object_set_new(root, "iout", io);
    }

    json_print_or_pretty(root, pretty);

    return 0;
  }

  if (!strcmp(sub, "temp")) {
    if (argc < 2) {
      usage_fault();
      return 2;
    }

    const char *sub2 = argv[1];
    if (!strcmp(sub2, "set")) {
      const char *ot_delay = NULL, *ut_delay = NULL;
      const char *ot_mode_s = NULL, *ut_mode_s = NULL;
      const char *ot_retries_s = NULL, *ut_retries_s = NULL;

      for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--ot-delay") && i + 1 < argc) {
          ot_delay = argv[++i];
          continue;
        }
        if (!strcmp(argv[i], "--ut-delay") && i + 1 < argc) {
          ut_delay = argv[++i];
          continue;
        }
        if (!strcmp(argv[i], "--ot-mode") && i + 1 < argc) {
          ot_mode_s = argv[++i];
          continue;
        }
        if (!strcmp(argv[i], "--ut-mode") && i + 1 < argc) {
          ut_mode_s = argv[++i];
          continue;
        }
        if (!strcmp(argv[i], "--ot-retries") && i + 1 < argc) {
          ot_retries_s = argv[++i];
          continue;
        }
        if (!strcmp(argv[i], "--ut-retries") && i + 1 < argc) {
          ut_retries_s = argv[++i];
          continue;
        }
      }

      uint8_t ot_mode = MODE_DISABLE_AND_RETRY, ut_mode = MODE_DISABLE_AND_RETRY; /* default */
      int ot_n = 4, ut_n = 4; /* default to 16s */
      int ot_retries = -1, ut_retries = -1; /* -1 means continuous */

      if (ot_mode_s && parse_mode(ot_mode_s, &ot_mode)) {
        fprintf(stderr, "bad --ot-mode\n");
        return 2;
      }
      if (ut_mode_s && parse_mode(ut_mode_s, &ut_mode)) {
        fprintf(stderr, "bad --ut-mode\n");
        return 2;
      }

      if (ot_delay) {
        int n = parse_n_from_arg(ot_delay);
        if (n < 0) {
          fprintf(stderr, "bad --ot-delay\n");
          return 2;
        }
        ot_n = n;
      }
      if (ut_delay) {
        int n = parse_n_from_arg(ut_delay);
        if (n < 0) {
          fprintf(stderr, "bad --ut-delay\n");
          return 2;
        }
        ut_n = n;
      }

      if (ot_retries_s) {
        if (!strcmp(ot_retries_s, "cont") || !strcmp(ot_retries_s, "continuous"))
          ot_retries = -1;
        else {
          char *end = NULL;
          long v = strtol(ot_retries_s, &end, 0);
          if (end == ot_retries_s || *end != '\0' || v < 0 || v > 6) {
            fprintf(stderr, "bad --ot-retries\n");
            return 2;
          }
          ot_retries = (int) v;
        }
      }

      if (ut_retries_s) {
        if (!strcmp(ut_retries_s, "cont") || !strcmp(ut_retries_s, "continuous"))
          ut_retries = -1;
        else {
          char *end = NULL;
          long v = strtol(ut_retries_s, &end, 0);
          if (end == ut_retries_s || *end != '\0' || v < 0 || v > 6) {
            fprintf(stderr, "bad --ut-retries\n");
            return 2;
          }
          ut_retries = (int) v;
        }
      }

      uint8_t ot = compose_resp_byte(ot_mode, ot_retries, ot_n);
      uint8_t ut = compose_resp_byte(ut_mode, ut_retries, ut_n);

      /* write both */
      int rc1 = pmbus_wr_byte(fd, PMBUS_OT_FAULT_RESPONSE, ot);
      int rc2 = pmbus_wr_byte(fd, PMBUS_UT_FAULT_RESPONSE, ut);

      json_t *out = json_object();
      json_t *w = json_object();
      json_object_set_new(w, "OT_FAULT_RESPONSE", json_integer(ot));
      json_object_set_new(w, "UT_FAULT_RESPONSE", json_integer(ut));
      json_object_set_new(out, "wrote", w);

      /* readback */
      json_t *rb = json_object();
      int v1 = pmbus_rd_byte(fd, PMBUS_OT_FAULT_RESPONSE);
      int v2 = pmbus_rd_byte(fd, PMBUS_UT_FAULT_RESPONSE);
      if (v1 >= 0)
        json_object_set_new(rb, "OT_FAULT_RESPONSE", json_integer(v1));
      if (v2 >= 0)
        json_object_set_new(rb, "UT_FAULT_RESPONSE", json_integer(v2));
      json_object_set_new(out, "readback", rb);

      json_print_or_pretty(out, pretty);

      return (rc1 < 0 || rc2 < 0) ? 1 : 0;
    }

    usage_fault();

    return 2;
  }

  usage_fault();

  return 2;
}
