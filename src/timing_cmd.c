/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "timing_cmd.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

static inline uint16_t
clamp16_ms(long v) {
  if (v < 0)
    v = 0;
  if (v > 32767)
    v = 32767;
  return (uint16_t) v;
}

static void
put_ms(json_t *o, const char *k, int fd, uint8_t cmd) {
  int v = pmbus_rd_word(fd, cmd);
  if (v >= 0)
    json_object_set_new(o, k, json_integer((uint16_t) v));
}

static int
write_word_maybe(int fd, const char *label, uint8_t cmd, const char *arg, json_t *delta) {
  if (!arg)
    return 0;

  char *end = NULL;
  errno = 0;

  long ms = strtol(arg, &end, 0);
  if (errno || end == arg || *end != '\0') {
    fprintf(stderr, "%s: invalid integer '%s'\n", label, arg);
    return 2;
  }

  uint16_t w = clamp16_ms(ms);
  if (pmbus_wr_word(fd, cmd, w) < 0) {
    perror(label);
    return 1;
  }
  json_object_set_new(delta, label, json_integer(w));

  return 0;
}

static int
write_fault_byte_from_fields(int fd, int resp_kind, int retries, int delay_sel, json_t *delta) {
  /* PMBus FAULT_RESPONSE (0x63) encodes as:
   * [7:6]=Response, [5:3]=Retries, [2:0]=Delay-units (device-specific units via MFR_RESPONSE_UNIT_CFG).
   * We simply pack them; caller ensures ranges.
   */
  if (resp_kind < 0 && retries < 0 && delay_sel < 0)
    return 0;

  uint8_t resp = (resp_kind < 0) ? 0 : (uint8_t)resp_kind; /* 0..3 */
  uint8_t rtry = (retries   < 0) ? 0 : (uint8_t)retries; /* 0..7 */
  uint8_t dsel = (delay_sel < 0) ? 3 : (uint8_t)delay_sel; /* 0..7 (3 is common default) */
  uint8_t b = (uint8_t)((resp & 3) << 6)
            | (uint8_t)((rtry & 7) << 3)
            | (dsel & 7);

  if (pmbus_wr_byte(fd, PMBUS_TON_MAX_FAULT_RESPONSE, b) < 0) {
    perror("TON_MAX_FAULT_RESPONSE");
    return 1;
  }
  json_object_set_new(delta, "TON_MAX_FAULT_RESPONSE", json_integer(b));

  return 0;
}

static int
write_fault_byte_literal(int fd, const char *hex, json_t *delta) {
  if (!hex)
    return 0;

  unsigned v = 0;
  if (sscanf(hex, "%i", (int *) &v) != 1 || v > 0xFF) {
    fprintf(stderr, "fault-byte: need 0..255 (e.g. 0x9B)\n");
    return 2;
  }
  if (pmbus_wr_byte(fd, PMBUS_TON_MAX_FAULT_RESPONSE, (uint8_t) v) < 0) {
    perror("TON_MAX_FAULT_RESPONSE");
    return 1;
  }
  json_object_set_new(delta, "TON_MAX_FAULT_RESPONSE", json_integer((int) v));

  return 0;
}

struct timing_profile {
  uint16_t ton_delay;
  uint16_t ton_rise;
  uint16_t ton_max_fault_limit;
  uint8_t ton_max_fault_response; /* packed byte */
  uint8_t __pad1;
  uint16_t toff_delay;
  uint16_t toff_fall;
  uint16_t toff_max_warn_limit;
};

static const struct timing_profile PROFILE_SAFE = {
  /* Safe general-purpose (close to defaults, bounded startup, retry a few times) */
  .ton_delay = 250,
  .ton_rise = 100,
  .ton_max_fault_limit = 50,
  .ton_max_fault_response = 0x9B, /* resp=10(disable+retry), retries=3, delay-sel=3 */
  .toff_delay = 5,
  .toff_fall = 20,
  .toff_max_warn_limit = 30
};

static const struct timing_profile PROFILE_SEQUENCED = {
  /* Staggered multi-rail */
  .ton_delay = 500,
  .ton_rise = 200,
  .ton_max_fault_limit = 100,
  .ton_max_fault_response = 0xAB, /* resp=10, retries=5, delay-sel=3 */
  .toff_delay = 20,
  .toff_fall = 50,
  .toff_max_warn_limit = 50
};

static const struct timing_profile PROFILE_FAST = {
  /* Latency-sensitive */
  .ton_delay = 10,
  .ton_rise = 20,
  .ton_max_fault_limit = 20,
  .ton_max_fault_response = 0xC3, /* resp=11(disable until clear), retries=0, delay-sel=3 */
  .toff_delay = 0,
  .toff_fall = 20,
  .toff_max_warn_limit = 30
};

static const struct timing_profile PROFILE_PREBIAS = {
  /* Soft-stop / pre-bias friendly */
  .ton_delay = 250,
  .ton_rise = 150,
  .ton_max_fault_limit = 50,
  .ton_max_fault_response = 0x9B,
  .toff_delay = 10,
  .toff_fall = 80,
  .toff_max_warn_limit = 100
};

static const struct timing_profile *
pick_profile(const char *name) {
  if (!name)
    return NULL;

  if (!strcmp(name, "safe"))
    return &PROFILE_SAFE;

  if (!strcmp(name, "sequenced"))
    return &PROFILE_SEQUENCED;

  if (!strcmp(name, "fast"))
    return &PROFILE_FAST;

  if (!strcmp(name, "prebias"))
    return &PROFILE_PREBIAS;

  return NULL;
}

static void
usage_timing(void) {
  fprintf(stderr,
"timing get\n"
"timing set [--profile safe|sequenced|fast|prebias]\n"
"           [--ton-delay MS] [--ton-rise MS] [--ton-max-fault MS]\n"
"           [--toff-delay MS] [--toff-fall MS] [--toff-max-warn MS]\n"
"           [--fault-byte 0xHH]\n"
"           [--fault-response disable-retry|disable-until-cleared|ignore]\n"
"           [--retries 0..7] [--delay-units 0..7]\n"
  );
}

int
cmd_timing(int fd, int argc, char *const *argv, int pretty) {
  if (argc < 1) {
    usage_timing();
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    json_t *o = json_object();
    put_ms(o, "TON_DELAY_ms", fd, PMBUS_TON_DELAY);
    put_ms(o, "TON_RISE_ms", fd, PMBUS_TON_RISE);
    put_ms(o, "TON_MAX_FAULT_LIMIT_ms", fd, PMBUS_TON_MAX_FAULT_LIMIT);

    int fr = pmbus_rd_byte(fd, PMBUS_TON_MAX_FAULT_RESPONSE);
    if (fr >= 0)
      json_object_set_new(o, "TON_MAX_FAULT_RESPONSE", json_integer(fr));

    put_ms(o, "TOFF_DELAY_ms", fd, PMBUS_TOFF_DELAY);
    put_ms(o, "TOFF_FALL_ms", fd, PMBUS_TOFF_FALL);
    put_ms(o, "TOFF_MAX_WARN_LIMIT_ms", fd, PMBUS_TOFF_MAX_WARN_LIMIT);

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    const char *profile = NULL;
    const char *ton_delay = NULL, *ton_rise = NULL, *ton_max = NULL;
    const char *toff_delay = NULL, *toff_fall = NULL, *toff_warn = NULL;
    const char *fault_byte = NULL;
    int resp_kind = -1;         /* -1 keep, 0 ignore, 2 disable+retry, 3 disable until cleared */
    int retries = -1;           /* 0..7 */
    int delay_sel = -1;         /* 0..7 */

    for (int i = 1; i < argc; i++) {
      const char *a = argv[i];
      if (!strcmp(a, "--profile") && i + 1 < argc)
        profile = argv[++i];
      else if (!strcmp(a, "--ton-delay") && i + 1 < argc)
        ton_delay = argv[++i];
      else if (!strcmp(a, "--ton-rise") && i + 1 < argc)
        ton_rise = argv[++i];
      else if (!strcmp(a, "--ton-max-fault") && i + 1 < argc)
        ton_max = argv[++i];
      else if (!strcmp(a, "--toff-delay") && i + 1 < argc)
        toff_delay = argv[++i];
      else if (!strcmp(a, "--toff-fall") && i + 1 < argc)
        toff_fall = argv[++i];
      else if (!strcmp(a, "--toff-max-warn") && i + 1 < argc)
        toff_warn = argv[++i];
      else if (!strcmp(a, "--fault-byte") && i + 1 < argc)
        fault_byte = argv[++i];
      else if (!strcmp(a, "--fault-response") && i + 1 < argc) {
        const char *v = argv[++i];
        if (!strcmp(v, "ignore"))
          resp_kind = 0;
        else if (!strcmp(v, "disable-retry"))
          resp_kind = 2;
        else if (!strcmp(v, "disable-until-cleared"))
          resp_kind = 3;
        else {
          fprintf(stderr, "fault-response: invalid\n");
          return 2;
        }
      } else if (!strcmp(a, "--retries") && i + 1 < argc) {
        retries = atoi(argv[++i]);
        if (retries < 0 || retries > 7) {
          fprintf(stderr, "retries 0..7\n");
          return 2;
        }
      } else if (!strcmp(a, "--delay-units") && i + 1 < argc) {
        delay_sel = atoi(argv[++i]);
        if (delay_sel < 0 || delay_sel > 7) {
          fprintf(stderr, "delay-units 0..7\n");
          return 2;
        }
      } else {
        usage_timing();
        return 2;
      }
    }

    json_t *delta = json_object();

    /* apply profile first (if any) */
    const struct timing_profile *p = pick_profile(profile);
    if (p) {
      if (pmbus_wr_word(fd, PMBUS_TON_DELAY, p->ton_delay) < 0) {
        perror("TON_DELAY");
        return 1;
      }
      if (pmbus_wr_word(fd, PMBUS_TON_RISE, p->ton_rise) < 0) {
        perror("TON_RISE");
        return 1;
      }
      if (pmbus_wr_word(fd, PMBUS_TON_MAX_FAULT_LIMIT, p->ton_max_fault_limit) < 0) {
        perror("TON_MAX_FAULT_LIMIT");
        return 1;
      }
      if (pmbus_wr_byte(fd, PMBUS_TON_MAX_FAULT_RESPONSE, p->ton_max_fault_response) < 0) {
        perror("TON_MAX_FAULT_RESPONSE");
        return 1;
      }
      if (pmbus_wr_word(fd, PMBUS_TOFF_DELAY, p->toff_delay) < 0) {
        perror("TOFF_DELAY");
        return 1;
      }
      if (pmbus_wr_word(fd, PMBUS_TOFF_FALL, p->toff_fall) < 0) {
        perror("TOFF_FALL");
        return 1;
      }
      if (pmbus_wr_word(fd, PMBUS_TOFF_MAX_WARN_LIMIT, p->toff_max_warn_limit) < 0) {
        perror("TOFF_MAX_WARN_LIMIT");
        return 1;
      }
      json_object_set_new(delta, "profile", json_string(profile));
    }

    /* then apply explicit overrides */
    int rc;

    if ((rc = write_word_maybe(fd, "TON_DELAY", PMBUS_TON_DELAY, ton_delay, delta)))
      return rc;

    if ((rc = write_word_maybe(fd, "TON_RISE", PMBUS_TON_RISE, ton_rise, delta)))
      return rc;

    if ((rc = write_word_maybe(fd, "TON_MAX_FAULT_LIMIT", PMBUS_TON_MAX_FAULT_LIMIT, ton_max, delta)))
      return rc;

    if ((rc = write_word_maybe(fd, "TOFF_DELAY", PMBUS_TOFF_DELAY, toff_delay, delta)))
      return rc;

    if ((rc = write_word_maybe(fd, "TOFF_FALL", PMBUS_TOFF_FALL, toff_fall, delta)))
      return rc;

    if ((rc = write_word_maybe(fd, "TOFF_MAX_WARN_LIMIT", PMBUS_TOFF_MAX_WARN_LIMIT, toff_warn, delta)))
      return rc;

    /* fault-response: either literal byte or composed fields */
    if (fault_byte) {
      if ((rc = write_fault_byte_literal(fd, fault_byte, delta)))
        return rc;
    } else {
      if ((rc = write_fault_byte_from_fields(fd, resp_kind, retries, delay_sel, delta)))
        return rc;
    }

    /* readback */
    json_t *after = json_object();
    put_ms(after, "TON_DELAY_ms", fd, PMBUS_TON_DELAY);
    put_ms(after, "TON_RISE_ms", fd, PMBUS_TON_RISE);
    put_ms(after, "TON_MAX_FAULT_LIMIT_ms", fd, PMBUS_TON_MAX_FAULT_LIMIT);

    int fr = pmbus_rd_byte(fd, PMBUS_TON_MAX_FAULT_RESPONSE);
    if (fr >= 0)
      json_object_set_new(after, "TON_MAX_FAULT_RESPONSE", json_integer(fr));

    put_ms(after, "TOFF_DELAY_ms", fd, PMBUS_TOFF_DELAY);
    put_ms(after, "TOFF_FALL_ms", fd, PMBUS_TOFF_FALL);
    put_ms(after, "TOFF_MAX_WARN_LIMIT_ms", fd, PMBUS_TOFF_MAX_WARN_LIMIT);

    json_t *out = json_object();
    json_object_set_new(out, "changed", delta);
    json_object_set_new(out, "readback", after);
    json_print_or_pretty(out, pretty);

    return 0;
  }

  usage_timing();

  return 2;
}
