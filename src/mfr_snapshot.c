/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"

#include "decoders.h"
#include <jansson.h>
#include <string.h>
#include <stdio.h>

static json_t *
decode_snapshot_block(int fd, const uint8_t *b, int n) {
  json_t *o = json_object();

  if (n < 32) {
    json_object_set_new(o, "error", json_string("short block"));
    return o;
  }

  int exp5 = 0;

  pmbus_get_vout_mode_exp(fd, &exp5);

  json_object_set_new(o, "vin_old_V", json_real(pmbus_lin11_to_double(le16(&b[0]))));
  json_object_set_new(o, "vout_old_V", json_real(pmbus_lin16u_to_double(le16(&b[2]), exp5)));
  json_object_set_new(o, "iout_old_A", json_real(pmbus_lin11_to_double(le16(&b[4]))));
  json_object_set_new(o, "duty_old_pct", json_real(pmbus_lin11_to_double(le16(&b[6]))));
  json_object_set_new(o, "vin_V", json_real(pmbus_lin11_to_double(le16(&b[8]))));
  json_object_set_new(o, "vout_V", json_real(pmbus_lin16u_to_double(le16(&b[10]), exp5)));
  json_object_set_new(o, "iout_A", json_real(pmbus_lin11_to_double(le16(&b[12]))));
  json_object_set_new(o, "temp1_C", json_real(pmbus_lin11_to_double(le16(&b[14]))));
  json_object_set_new(o, "temp2_C", json_real(pmbus_lin11_to_double(le16(&b[16]))));
  json_object_set_new(o, "time_in_operation_s", json_integer(le16(&b[18])));
  json_object_set_new(o, "status_word", json_integer(le16(&b[20])));
  json_object_set_new(o, "status_byte", json_integer(b[22]));
  json_object_set_new(o, "status_vout", decode_status_vout(b[23]));
  json_object_set_new(o, "status_iout", decode_status_iout(b[24]));
  json_object_set_new(o, "status_vin", decode_status_input(b[25]));
  json_object_set_new(o, "status_temperature", decode_status_temperature(b[26]));
  json_object_set_new(o, "status_cml", decode_status_cml(b[27]));
  json_object_set_new(o, "snapshot_cycles", json_integer(le32(&b[28])));

  return o;
}

int
cmd_snapshot(int fd, int argc, char * const *argv) {
  int cycle = -1;
  bool decode = false;

  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "--cycle") && i + 1 < argc)
      cycle = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--decode"))
      decode = true;
  }

  if (cycle >= 0) {
    if (cycle > 19) {
      fprintf(stderr, "--cycle 0..19\n");
      return 2;
    }
    if (pmbus_wr_byte(fd, MFR_SNAPSHOT_CYCLES_SELECT, (uint8_t) cycle) < 0) {
      perror("MFR_SNAPSHOT_CYCLES_SELECT");
      return 1;
    }
  }

  uint8_t blk[64];
  int n = pmbus_rd_block(fd, MFR_GET_SNAPSHOT, blk, sizeof blk);
  if (n < 0) {
    perror("MFR_GET_SNAPSHOT");
    return 1;
  }

  json_t *o = json_object();
  json_object_set_new(o, "len", json_integer(n));

  char *hex = malloc(n * 2 + 1);
  for (int i = 0; i < n; i++)
    sprintf(hex + 2 * i, "%02X", blk[i]);
  hex[n * 2] = '\0';

  json_object_set_new(o, "hex", json_string(hex));
  free(hex);

  if (decode && n >= 32) {
    json_object_set_new(o, "decoded", decode_snapshot_block(fd, blk, n));
  }

  char *dump = json_dumps(o, JSON_INDENT(2));
  puts(dump);
  free(dump);
  json_decref(o);

  return 0;
}
