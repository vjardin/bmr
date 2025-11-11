/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "pmbus_io.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* See BMR480 specs */
#define BIT_PEC     (1u<<7)     /* Require Packet Error Check */
#define BIT_HRR     (1u<<6)     /* Hybrid Regulated Ratio enable */
#define BIT_DLS     (1u<<5)     /* 0: linear droop, 1: non-linear droop */
#define BIT_ARTDLC  (1u<<3)     /* Adaptive Ramp-up Time / Dynamic Loop Compensation enable */
#define BIT_DBV     (1u<<2)     /* Dynamic Bus Voltage enable */

static void
usage_hrr_short(void) {
  fprintf(stderr,
"hrr get\n"
"hrr set [--pec on|off] [--hrr on|off] [--dls linear|nonlinear] [--artdlc on|off] [--dbv on|off]\n"
"hrr set --raw 0xNN\n" "hrr help\n"
  );
}

static void
usage_hrr_long(void) {
  fprintf(stderr,
"\n"
"NAME\n"
"  hrr — Inspect and set MFR_SPECIAL_OPTIONS (0xE0) on Flex BMR480\n"
"\n"
"SYNOPSIS\n"
"  hrr get\n"
"  hrr set [--pec on|off] [--hrr on|off] [--dls linear|nonlinear] [--artdlc on|off] [--dbv on|off]\n"
"  hrr set --raw 0xNN\n"
"\n"
"DESCRIPTION (0xE0 is a R/W BYTE)\n"
"  Bit 7  (Require PEC)             : 0=Disabled, 1=Enabled.\n"
"                                     When enabled, the module expects SMBus PEC (CRC-8)\n"
"                                     on transactions. Ensure userland I2C stack sends PEC.\n"
"  Bit 6  (HRR enable)              : 0=Disabled, 1=Enabled.\n"
"                                     Hybrid Regulated Ratio. HRR threshold uses VIN_UV_WARN_LIMIT (0x58).\n"
"  Bit 5  (DLS slope configuration) : 0=Linear droop, 1=Non-linear droop.\n"
"  Bit 4  (Reserved)                : Keep at 0.\n"
"  Bit 3  (ART/DLC enable)          : 0=Disabled, 1=Enabled. Adaptive Ramp-up / Dynamic Loop Comp.\n"
"  Bit 2  (DBV enable)              : 0=Disabled, 1=Enabled. Dynamic Bus Voltage.\n"
"  Bits 1..0 (Reserved)             : Keep at 0.\n"
"\n"
"OUTPUT (JSON) - TBC\n"
"  {\n"
"    \"MFR_SPECIAL_OPTIONS_raw\": <byte>,\n"
"    \"require_pec\": true|false,\n"
"    \"hrr_enabled\": true|false,\n"
"    \"dls_mode\": \"linear\"|\"nonlinear\",\n"
"    \"artdlc_enabled\": true|false,\n"
"    \"dbv_enabled\": true|false\n"
"  }\n"
"\n"
"EXAMPLES\n"
"  # Inspect current options\n"
"  bmr hrr get\n"
"\n"
"  # Enable HRR, set non-linear droop, and turn on ART/DLC; leave others unchanged\n"
"  bmr hrr set --hrr on --dls nonlinear --artdlc on\n"
"\n"
"  # Require PEC (I2C userland must send PEC!)\n"
"  bmr hrr set --pec on\n"
"\n"
"  # Direct raw write: HRR+PEC enabled (bits 6 and 7), others 0 => 0xC0\n"
"  bmr hrr set --raw 0xC0\n"
"\nNOTES\n"
"  * Some BMR families/revisions mark certain bits as Reserved. Writing them may NACK.\n"
"  * Enabling PEC without sending PEC from software will break communication.\n"
"  * HRR behavior depends on VIN_UV_WARN_LIMIT (0x58).\n"
  );
}

static int
parse_onoff(const char *s) {
  if (!s)
    return -1;
  if (!strcmp(s, "on") || !strcmp(s, "enable") || !strcmp(s, "enabled"))
    return 1;
  if (!strcmp(s, "off") || !strcmp(s, "disable") || !strcmp(s, "disabled"))
    return 0;

  return -1;
}

int
cmd_hrr(int fd, int argc, char *const *argv, int pretty) {
  if (argc == 0) {
    usage_hrr_short();
    return 2;
  }

  if (!strcmp(argv[0], "help") || !strcmp(argv[0], "--help") || !strcmp(argv[0], "-h")) {
    usage_hrr_long();
    return 0;
  }

  if (!strcmp(argv[0], "get")) {
    int v = pmbus_rd_byte(fd, MFR_SPECIAL_OPTIONS);
    if (v < 0) {
      perror("MFR_SPECIAL_OPTIONS");
      return 1;
    }

    json_t *o = json_object();
    json_object_set_new(o, "MFR_SPECIAL_OPTIONS_raw", json_integer((unsigned) (uint8_t) v));
    json_object_set_new(o, "require_pec", json_boolean(!!((uint8_t) v & BIT_PEC)));
    json_object_set_new(o, "hrr_enabled", json_boolean(!!((uint8_t) v & BIT_HRR)));
    json_object_set_new(o, "dls_mode", json_string(((uint8_t) v & BIT_DLS) ? "nonlinear" : "linear"));
    json_object_set_new(o, "artdlc_enabled", json_boolean(!!((uint8_t) v & BIT_ARTDLC)));
    json_object_set_new(o, "dbv_enabled", json_boolean(!!((uint8_t) v & BIT_DBV)));

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    const char *raw_s = NULL, *pec_s = NULL, *hrr_s = NULL, *dls_s = NULL, *art_s = NULL, *dbv_s = NULL;

    for (int i = 1; i < argc; i++) {
      const char *a = argv[i];
      if (!strcmp(a, "--raw") && i + 1 < argc)
        raw_s = argv[++i];
      else if (!strcmp(a, "--pec") && i + 1 < argc)
        pec_s = argv[++i];
      else if (!strcmp(a, "--hrr") && i + 1 < argc)
        hrr_s = argv[++i];
      else if (!strcmp(a, "--dls") && i + 1 < argc)
        dls_s = argv[++i];
      else if (!strcmp(a, "--artdlc") && i + 1 < argc)
        art_s = argv[++i];
      else if (!strcmp(a, "--dbv") && i + 1 < argc)
        dbv_s = argv[++i];
      else if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
        usage_hrr_long();
        return 0;
      }
    }

    int cur = pmbus_rd_byte(fd, MFR_SPECIAL_OPTIONS);
    if (cur < 0) {
      perror("MFR_SPECIAL_OPTIONS read");
      return 1;
    }
    uint8_t nv = (uint8_t) cur;

    if (raw_s) {
      char *end = NULL;
      errno = 0;
      long v = strtol(raw_s, &end, 0);
      if (errno || !end || *end || v < 0 || v > 0xFF) {
        usage_hrr_short();
        return 2;
      }
      nv = (uint8_t) v;
    } else {
      int v;
      if ((v = parse_onoff(pec_s)) >= 0) {
        if (v)
          nv |= BIT_PEC;
        else
          nv &= ~BIT_PEC;
      }
      if ((v = parse_onoff(hrr_s)) >= 0) {
        if (v)
          nv |= BIT_HRR;
        else
          nv &= ~BIT_HRR;
      }

      if (dls_s) {
        if (!strcmp(dls_s, "linear"))
          nv &= ~BIT_DLS;
        else if (!strcmp(dls_s, "nonlinear"))
          nv |= BIT_DLS;
        else {
          fprintf(stderr, "--dls linear|nonlinear\n");
          return 2;
        }
      }

      if ((v = parse_onoff(art_s)) >= 0) {
        if (v)
          nv |= BIT_ARTDLC;
        else
          nv &= ~BIT_ARTDLC;
      }
      if ((v = parse_onoff(dbv_s)) >= 0) {
        if (v)
          nv |= BIT_DBV;
        else
          nv &= ~BIT_DBV;
      }
    }

    if (nv != (uint8_t) cur) {
      if (pmbus_wr_byte(fd, MFR_SPECIAL_OPTIONS, nv) < 0) {
        perror("MFR_SPECIAL_OPTIONS write");
        return 1;
      }
    }

    int rb = pmbus_rd_byte(fd, MFR_SPECIAL_OPTIONS);
    if (rb < 0) {
      perror("MFR_SPECIAL_OPTIONS readback");
      return 1;
    }

    json_t *o = json_object();
    json_object_set_new(o, "changed", json_boolean(nv != (uint8_t) cur));
    json_object_set_new(o, "MFR_SPECIAL_OPTIONS_raw", json_integer((unsigned) (uint8_t) rb));
    json_object_set_new(o, "require_pec", json_boolean(!!((uint8_t) rb & BIT_PEC)));
    json_object_set_new(o, "hrr_enabled", json_boolean(!!((uint8_t) rb & BIT_HRR)));
    json_object_set_new(o, "dls_mode", json_string(((uint8_t) rb & BIT_DLS) ? "nonlinear" : "linear"));
    json_object_set_new(o, "artdlc_enabled", json_boolean(!!((uint8_t) rb & BIT_ARTDLC)));
    json_object_set_new(o, "dbv_enabled", json_boolean(!!((uint8_t) rb & BIT_DBV)));

    json_print_or_pretty(o, pretty);

    return 0;
  }

  usage_hrr_short();

  return 2;
}
