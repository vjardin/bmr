/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int
cmd_user_data(int fd, int argc, char *const *argv, int pretty) {
  if (argc < 1) {
    fprintf(stderr, "user-data get|set ...\n");
    return 2;
  }

  if (!strcmp(argv[0], "get")) {
    uint8_t buf[64];
    int n = pmbus_rd_block(fd, MFR_USER_DATA_00, buf, sizeof buf);
    if (n < 0) {
      perror("USER_DATA_00");
      return 1;
    }

    json_t *o = json_object();
    json_object_set_new(o, "len", json_integer(n));
    json_object_set_new(o, "ascii", json_stringn((char *) buf, n));
    json_add_hex_ascii(o, "hex", buf, (size_t)n);

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(argv[0], "set")) {
    const char *hex = NULL, *ascii = NULL;
    bool store = false, restore = false;

    for (int i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "--hex") && i + 1 < argc)
        hex = argv[++i];
      else if (!strcmp(argv[i], "--ascii") && i + 1 < argc)
        ascii = argv[++i];
      else if (!strcmp(argv[i], "--store"))
        store = true;
      else if (!strcmp(argv[i], "--restore"))
        restore = true;
    }

    uint8_t b[64];
    int n = 0;

    if (hex) {
      size_t L = strlen(hex);
      if (L % 2) {
        fprintf(stderr, "hex even length\n");
        return 2;
      }
      if (L / 2 > 32) {
        fprintf(stderr, "max 32 bytes\n");
        return 2;
      }
      for (size_t i = 0; i < L / 2; i++) {
        unsigned v;
        sscanf(hex + 2 * i, "%2x", &v);
        b[i] = (uint8_t) v;
      }
      n = (int) (L / 2);
    } else if (ascii) {
      n = (int) strlen(ascii);
      if (n > 32)
        n = 32;
      memcpy(b, ascii, n);
    } else {
      fprintf(stderr, "need --hex or --ascii\n");

      return 2;
    }

    if (pmbus_wr_block(fd, MFR_USER_DATA_00, b, n) < 0) {
      perror("USER_DATA_00 write");

      return 1;
    }

    if (store)
      pmbus_send_byte(fd, PMBUS_STORE_USER_ALL);
    if (restore)
      pmbus_send_byte(fd, PMBUS_RESTORE_USER_ALL);

    puts("OK");

    return 0;
  }

  fprintf(stderr, "user-data get|set ...\n");

  return 2;
}
