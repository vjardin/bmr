/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>

static json_t *
decode(uint8_t v) {
  json_t *o = json_object();

  int mode = (v >> 6) & 3;
  const char *m =
      (mode == 0) ? "standalone"
    : (mode == 2) ? "dls"
    : (mode == 1) ? "slave(n/a)"
    : "master(n/a)";

  json_object_set_new(o, "raw", json_integer(v));
  json_object_set_new(o, "mode", json_string(m));
  json_object_set_new(o, "pg_highz_when_active", json_boolean(!!(v & 0x20)));
  json_object_set_new(o, "pg_enable", json_boolean(!!(v & 0x04)));
  json_object_set_new(o, "sec_rc_pull_enable", json_boolean(!!(v & 0x01)));

  return o;
}

static int
encode(const char *mode, const char *pg, int pg_en, int sec_rc, uint8_t *out) {
  uint8_t v = 0;

  if (!mode)
    mode = "standalone";
  if (!pg)
    pg = "pushpull";
  if (!strcmp(mode, "standalone"))
    v |= (0 << 6);
  else if (!strcmp(mode, "dls"))
    v |= (2 << 6);
  else if (!strcmp(mode, "slave"))
    v |= (1 << 6);
  else if (!strcmp(mode, "master"))
    v |= (3 << 6);
  else
    return -1;

  if (!strcmp(pg, "highz"))
    v |= (1 << 5);
  else if (!strcmp(pg, "pushpull")) {
    /* noop */
  } else
    return -2;

  if (pg_en)
    v |= (1 << 2);

  if (sec_rc)
    v |= 1;

  *out = v;

  return 0;
}

int
cmd_multipin(int fd, int argc, char * const * argv, int pretty) {
  if (argc < 1) {
    fprintf(stderr, "mfr-multi-pin get|set ...\n");
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    int v = pmbus_rd_byte(fd, MFR_MULTI_PIN_CONFIG);
    if (v < 0) {
      perror("MFR_MULTI_PIN_CONFIG");
      return 1;
    }

    json_t *o = decode((uint8_t) v);

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    const char *mode = NULL, *pg = NULL;
    int pg_en = -1, sec = -1;

    for (int i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--mode") && i + 1 < argc)
        mode = argv[++i];
      else if (!strcmp(argv[i], "--pg") && i + 1 < argc)
        pg = argv[++i];
      else if (!strcmp(argv[i], "--pg-enable") && i + 1 < argc)
        pg_en = atoi(argv[++i]);
      else if (!strcmp(argv[i], "--sec-rc-pull") && i + 1 < argc)
        sec = atoi(argv[++i]);
    }

    uint8_t v;
    int rc = encode(mode, pg, pg_en > 0, sec > 0, &v);
    if (rc < 0) {
      fprintf(stderr, "invalid args\n");
      return 2;
    }
    if (pmbus_wr_byte(fd, MFR_MULTI_PIN_CONFIG, v) < 0) {
      perror("write MFR_MULTI_PIN_CONFIG");
      return 1;
    }

    json_t *o = decode(v);
    json_object_set_new(o, "result", json_string("OK"));
    json_print_or_pretty(o, pretty);

    return 0;
  }

  fprintf(stderr, "mfr-multi-pin get|set ...\n");

  return 2;
}
