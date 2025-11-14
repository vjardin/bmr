/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "util_json.h"
#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


static void
decode_rwb(uint8_t v, json_t *o) {
  json_object_set_new(o, "raw", json_integer(v));
}

static void
decode_rww(uint16_t v, json_t *o) {
  json_object_set_new(o, "raw", json_integer(v));
}

static void
usage_rw(void) {
  fprintf(stderr,
"rw get [byte|word] [--cmd 0xHH]\n"
"rw set [byte|word] [--cmd 0xHH] [--value 0xAAAA]\n"
  );
}

static int
parse_cmd(const char *s, uint8_t *out) {
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

static int
parse_value(const char *s, uint16_t *out) {
  if (!s)
    return -1;

  char *end = NULL;
  errno = 0;
  long v = strtol(s, &end, 0);
  if (errno || end == s || *end != '\0' || v < 0 || v > 0xFFFF)
    return -1;

  *out = (uint16_t) v;

  return 0;
}

int
cmd_rw(int fd, int argc, char *const *argv, int pretty) {
  const char *cmd = NULL;     /* 0xHH */
  const char *value = NULL;    /* 0xAAAA */
  uint8_t cmdv;
  uint16_t valuev;

  if (argc < 4) {
    fprintf(stderr, "not enough agrs %d\n", argc);
    usage_rw();
    return 2;
  }

  for (int i = 2; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--cmd") && i + 1 < argc)
      cmd = argv[++i];
    else if (!strcmp(a, "--value") && i + 1 < argc)
      value = argv[++i];
    else {
      fprintf(stderr, "unknown args %s\n", a);
      usage_rw();
      return 2;
    }
  }

  if (cmd == NULL) {
    fprintf(stderr, "--cmd missing\n");
    usage_rw();
    return 2;
  }

  if (parse_cmd(cmd, &cmdv) != 0) {
    fprintf(stderr, "--cmd expects 0..255 (e.g. 0x80)\n");
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    if (!strcmp(argv[1], "byte")) {

      int val = pmbus_rd_byte(fd, cmdv);
      if (val < 0) {
        perror("RW");
        return 1;
      }
      json_t *o = json_object();
      decode_rwb((uint8_t) val, o);
      json_print_or_pretty(o, pretty);
      return 0;
    } else if (!strcmp(argv[1], "word")) {

      int val = pmbus_rd_word(fd, cmdv);
      if (val < 0) {
        perror("RW");
        return 1;
      }
      json_t *o = json_object();
      decode_rww((uint16_t) val, o);
      json_print_or_pretty(o, pretty);
      return 0;
    } else {
      fprintf(stderr, "wrong second args %s\n", argv[1]);
      usage_rw();
      return 2;
    }
  }

  if (!strcmp(argv[0], "set")) {

    if (value == NULL) {
      fprintf(stderr, "--value missing\n");
      usage_rw();
      return 2;
    }

    if (parse_value(value, &valuev) != 0) {
      fprintf(stderr, "--vlaue expects 0..65535 (e.g. 0x8080)\n");
      return 2;
    }

    if (!strcmp(argv[1], "byte")) {

      int val = pmbus_wr_byte(fd, cmdv, (uint8_t) valuev);
      if (val < 0) {
        perror("RW");
        return 1;
      }
      puts("OK");

      return 0;
    } else if (!strcmp(argv[1], "word")) {

      int val = pmbus_wr_word(fd, cmdv, valuev);
      if (val < 0) {
        perror("RW");
        return 1;
      }
      puts("OK");

      return 0;
    } else {
      fprintf(stderr, "wrong second args %s\n", argv[1]);
      usage_rw();
      return 2;
    }
  }

  fprintf(stderr, "wrong first args %s\n", argv[0]);

  usage_rw();

  return 2;
}
