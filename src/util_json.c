/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "util_json.h"
#include "pmbus_io.h"

#include <jansson.h>

/* TODO: replace with json_add_len_and_hex() */
static void
add_hex_ascii(json_t *dst, const uint8_t *buf, int n) {
  json_object_set_new(dst, "len", json_integer(n));
  json_object_set_new(dst, "ascii", json_stringn((const char *) buf, n));

  char *hex = (char *) malloc((size_t) n * 2 + 1);

  for (int i = 0; i < n; i++)
    sprintf(hex + 2 * i, "%02X", buf[i]);
  hex[n * 2] = '\0';

  json_object_set_new(dst, "hex", json_string(hex));

  free(hex);
}

void
rd_block_string(int fd, uint8_t cmd, const char *key, json_t *root) {
  uint8_t b[64];
  int n = pmbus_rd_block(fd, cmd, b, (int) sizeof b);
  if (n < 0)
    return;
  json_t *o = json_object();
  add_hex_ascii(o, b, n);

  json_object_set_new(root, key, o);
}

void
json_print_or_pretty(json_t *o, int pretty) {
  if (!o)
    return;

  char *s = json_dumps(o, pretty ? JSON_INDENT(2) | JSON_SORT_KEYS : JSON_SORT_KEYS);

  if (s) {
    puts(s);
    free(s);
  } else {
    puts("Invalid json object");
  }
  json_decref(o);
}

int
json_add_hex_ascii(json_t *dst, const char *key, const void *bufv, size_t n) {
  if (!dst || !key || (!bufv && n))
    return -1;
  if (n > (SIZE_MAX / 2))
    return -1;

  const uint8_t *buf = (const uint8_t *) bufv;
  size_t outlen = n * 2;

  char *hex = (char *) malloc(outlen + 1);
  if (!hex)
    return -1;

  static const char *HD = "0123456789ABCDEF";
  /* avoid using sprintf() */
  for (size_t i = 0; i < n; ++i) {
    hex[(i << 1)] = HD[(buf[i] >> 4) & 0xF];
    hex[(i << 1) + 1] = HD[buf[i] & 0xF];
  }
  hex[outlen] = '\0';

  json_t *s = json_stringn(hex, outlen);
  free(hex);

  if (!s)
    return -1;

  return json_object_set_new(dst, key, s);
}

int
json_add_len_and_hex(json_t *dst, const char *key, const void *buf, size_t n) {
  if (!dst)
    return -1;

  if (json_object_set_new(dst, "len", json_integer((json_int_t) n)) != 0)
    return -1;

  return json_add_hex_ascii(dst, key, buf, n);
}
