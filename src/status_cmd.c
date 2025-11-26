/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "status_cmd.h"
#include "decoders.h"

#include "util_json.h"
#include <jansson.h>

int
cmd_status(int fd, int argc, char *const *argv, int pretty) {
  (void)argc;
  (void)argv;

  json_t *o = json_object();

  int sb = pmbus_rd_byte(fd, PMBUS_STATUS_BYTE);
  int sw = pmbus_rd_word(fd, PMBUS_STATUS_WORD);
  int sv = pmbus_rd_byte(fd, PMBUS_STATUS_VOUT);
  int si = pmbus_rd_byte(fd, PMBUS_STATUS_IOUT);
  int siu = pmbus_rd_byte(fd, PMBUS_STATUS_INPUT);
  int st = pmbus_rd_byte(fd, PMBUS_STATUS_TEMPERATURE);
  int sc = pmbus_rd_byte(fd, PMBUS_STATUS_CML);

  if (sb >= 0)
    json_object_set_new(o, "STATUS_BYTE", decode_status_byte((uint8_t) sb));
  if (sw >= 0)
    json_object_set_new(o, "STATUS_WORD", decode_status_word((uint16_t) sw));
  if (sv >= 0)
    json_object_set_new(o, "STATUS_VOUT", decode_status_vout((uint8_t) sv));
  if (si >= 0)
    json_object_set_new(o, "STATUS_IOUT", decode_status_iout((uint8_t) si));
  if (siu >= 0)
    json_object_set_new(o, "STATUS_INPUT", decode_status_input((uint8_t) siu));
  if (st >= 0)
    json_object_set_new(o, "STATUS_TEMPERATURE", decode_status_temperature((uint8_t) st));
  if (sc >= 0)
    json_object_set_new(o, "STATUS_CML", decode_status_cml((uint8_t) sc));

  json_print_or_pretty(o, pretty);

  return 0;
}
