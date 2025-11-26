/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmbus_io.h"
#include "capability_cmd.h"
#include "util_json.h"

#include <jansson.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void
usage_cap_short(void) {
  fprintf(stderr,
"capability get\n"
"capability check [--need-pec on|off] [--min-speed 100|400|1000] [--need-alert on|off]\n"
"                 [--need-fp on|off] [--need-avsbus on|off] [--strict]\n"
"capability help\n"
  );
}

static void
usage_cap_long(void) {
  fprintf(stderr,
"\n"
"NAME\n"
"  capability — Decode PMBUS_CAPABILITY (0x19) and optionally check requirements\n"
"\n"
"SYNOPSIS\n"
"  capability get\n"
"  capability check [--need-pec on|off] [--min-speed 100|400|1000]\n"
"                   [--need-alert on|off] [--need-fp on|off] [--need-avsbus on|off]\n"
"                   [--strict]\n"
"\n"
"DESCRIPTION (0x19 is a READ BYTE)\n"
"  Bit 7   : PEC support (1 = device supports SMBus Packet Error Checking)\n"
"  Bits 6:5: Max bus speed code (00=100 kHz, 01=400 kHz, 10=1 MHz, 11=reserved)\n"
"  Bit 4   : SMBALERT# support (1 = device supports alert protocol/pin)\n"
"  Bit 3   : Numeric format (0 = LINEAR/ULINEAR/DIRECT; 1 = IEEE-754 half-precision)\n"
"  Bit 2   : AVSBus support (1 = device supports AVSBus)\n"
"  Bits 1:0: Reserved (should be 0)\n"
"\n"
"OUTPUT (JSON) TBC\n"
"  capability get ->\n"
"    {\n"
"      \"capability\": {\n"
"        \"raw\": <byte>,\n"
"        \"pec_supported\": true|false,\n"
"        \"max_bus_speed\": { \"code\": 0|1|2|3, \"khz\": 100|400|1000|null, \"text\": \"...\" },\n"
"        \"smbalert_supported\": true|false,\n"
"        \"numeric_format\": \"linear/direct\" | \"ieee754_half\",\n"
"        \"avsbus_supported\": true|false,\n"
"        \"reserved_low_bits\": <0..3>\n"
"      }\n"
"    }\n"
"\n"
"  capability check -> adds\n"
"    \"checks\": {\n"
"      \"pec_ok\": bool, \"bus_speed_ok\": bool, \"alert_ok\": bool,\n"
"      \"numeric_format_ok\": bool, \"avsbus_ok\": bool,\n"
"      \"reserved_low_zero\": bool, \"speed_code_valid\": bool\n"
"    },\n"
"    \"mismatches\": [ list of failed checks ]\n"
"\n"
"NOTES\n"
"  * --strict fails if reserved low bits (1..0) are non-zero and if speed code==3 (reserved).\n"
"  * This command intentionally does NOT include any MFR_* identity fields.\n"
"\n"
  );
}

static const char *
speed_text(unsigned code, int *khz_out) {
  switch (code & 0x3u) {
  case 0:
    if (khz_out)
      *khz_out = 100;
    return "100 kHz";
  case 1:
    if (khz_out)
      *khz_out = 400;
    return "400 kHz";
  case 2:
    if (khz_out)
      *khz_out = 1000;
    return "1 MHz";
  case 3:
  default:
    if (khz_out)
      *khz_out = -1;
    return "reserved";
  }
}

static void
decode_cap(uint8_t cap, json_t *dst) {
  unsigned pec = (cap >> 7) & 0x1u;
  unsigned spd = (cap >> 5) & 0x3u;
  unsigned sal = (cap >> 4) & 0x1u;
  unsigned num = (cap >> 3) & 0x1u;
  unsigned avs = (cap >> 2) & 0x1u;
  unsigned rsv = cap & 0x03u; /* only bits [1:0] are reserved */

  int khz = -1;
  const char *txt = speed_text(spd, &khz);

  json_object_set_new(dst, "raw", json_integer(cap));
  json_object_set_new(dst, "pec_supported", json_boolean(pec));

  json_t *bs = json_object();
  json_object_set_new(bs, "code", json_integer(spd));
  json_object_set_new(bs, "khz", (khz > 0) ? json_integer(khz) : json_null());
  json_object_set_new(bs, "text", json_string(txt));
  json_object_set_new(dst, "max_bus_speed", bs);

  json_object_set_new(dst, "smbalert_supported", json_boolean(sal));
  json_object_set_new(dst, "numeric_format", json_string(num ? "ieee754_half" : "linear/direct"));
  json_object_set_new(dst, "avsbus_supported", json_boolean(avs));
  json_object_set_new(dst, "reserved_low_bits", json_integer(rsv));
}

static void
add_check(json_t *checks, const char *k, int ok, json_t *mism) {
  json_object_set_new(checks, k, json_boolean(ok));
  if (!ok)
    json_array_append_new(mism, json_string(k));
}

static int
parse_onoff(const char *s, int *out) {
  if (!s || !out)
    return -1;

  if (!strcmp(s, "on") || !strcmp(s, "yes") || !strcmp(s, "true")) {
    *out = 1;
    return 0;
  }
  if (!strcmp(s, "off") || !strcmp(s, "no") || !strcmp(s, "false")) {
    *out = 0;
    return 0;
  }

  return -1;
}

int
cmd_capability(int fd, int argc, char *const *argv, int pretty) {
  if (argc == 0) {
    usage_cap_short();
    return 2;
  }

  if (!strcmp(argv[0], "help") || !strcmp(argv[0], "--help") || !strcmp(argv[0], "-h")) {
    usage_cap_long();
    return 0;
  }

  if (!strcmp(argv[0], "get")) {
    int v = pmbus_rd_byte(fd, PMBUS_CAPABILITY);
    if (v < 0) {
      perror("PMBUS_CAPABILITY");
      return 1;
    }

    json_t *out = json_object();
    json_t *capo = json_object();
    decode_cap((uint8_t) v, capo);
    json_object_set_new(out, "capability", capo);

    json_print_or_pretty(out, pretty);

    return 0;
  }

  /* capability check [opts] */
  if (!strcmp(argv[0], "check")) {
    const char *need_pec = NULL, *need_alert = NULL, *min_speed = NULL;
    const char *need_fp = NULL, *need_avsbus = NULL;
    int strict = 0;

    for (int i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--need-pec") && i + 1 < argc)
        need_pec = argv[++i];
      else if (!strcmp(argv[i], "--need-alert") && i + 1 < argc)
        need_alert = argv[++i];
      else if (!strcmp(argv[i], "--min-speed") && i + 1 < argc)
        min_speed = argv[++i];
      else if (!strcmp(argv[i], "--need-fp") && i + 1 < argc)
        need_fp = argv[++i];
      else if (!strcmp(argv[i], "--need-avsbus") && i + 1 < argc)
        need_avsbus = argv[++i];
      else if (!strcmp(argv[i], "--strict"))
        strict = 1;
      else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
        usage_cap_long();
        return 0;
      }
    }

    int v = pmbus_rd_byte(fd, PMBUS_CAPABILITY);
    if (v < 0) {
      perror("PMBUS_CAPABILITY");
      return 1;
    }
    uint8_t cap = (uint8_t) v;

    json_t *out = json_object();
    json_t *capo = json_object();
    decode_cap(cap, capo);
    json_object_set_new(out, "capability", capo);

    json_t *checks = json_object();
    json_t *mism = json_array();

    /* PEC check */
    if (need_pec) {
      int want, have = !!(cap & 0x80u);
      if (parse_onoff(need_pec, &want) == 0)
        add_check(checks, "pec_ok", (want == have), mism);
    }

    /* Speed check */
    if (min_speed) {
      int req = atoi(min_speed);  /* expect 100 or 400 or 1000 */
      unsigned code = (cap >> 5) & 0x3u;
      int khz = -1;
      (void) speed_text(code, &khz);
      int ok = (khz >= req);    /* reserved -> khz==-1 -> fail */
      add_check(checks, "bus_speed_ok", ok, mism);
    }

    /* ALERT check */
    if (need_alert) {
      int want, have = !!(cap & 0x10u);
      if (parse_onoff(need_alert, &want) == 0)
        add_check(checks, "alert_ok", (want == have), mism);
    }

    /* Numeric format check (bit 3) */
    if (need_fp) {
      int want_fp, have_fp = !!(cap & 0x08u);
      if (parse_onoff(need_fp, &want_fp) == 0)
        add_check(checks, "numeric_format_ok", (want_fp == have_fp), mism);
    }

    /* AVSBus check (bit 2) */
    if (need_avsbus) {
      int want_avs, have_avs = !!(cap & 0x04u);
      if (parse_onoff(need_avsbus, &want_avs) == 0)
        add_check(checks, "avsbus_ok", (want_avs == have_avs), mism);
    }

    /* Reserved bits strictness */
    if (strict) {
      add_check(checks, "reserved_low_zero", (cap & 0x03u) == 0, mism);

      add_check(checks, "speed_code_valid", ((cap >> 5) & 0x3u) != 3u, mism);
    }

    json_object_set_new(out, "checks", checks);
    json_object_set_new(out, "mismatches", mism);

    json_print_or_pretty(out, pretty);

    return 0;
  }

  usage_cap_short();

  return 2;
}
