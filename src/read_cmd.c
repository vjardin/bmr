/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "read_cmd.h"
#include "util_json.h"

#include <jansson.h>
#include <string.h>
#include <stdio.h>

static void
out_double(const char *k, double v, int pretty) {
  json_t *o = json_object();
  json_object_set_new(o, k, json_real(v));
  json_print_or_pretty(o, pretty);
}

enum enc_t { ENC_LIN11, ENC_LIN16U, ENC_RAW_INT };

static void
add_read_field(json_t *o, const char *key, int fd, uint8_t reg, enum enc_t enc, int exp5) {
  int w = pmbus_rd_word(fd, reg);
  if (w < 0)
    return;

  switch (enc) {
  case ENC_LIN11:
    json_object_set_new(o, key, json_real(pmbus_lin11_to_double((uint16_t)w)));
    break;
  case ENC_LIN16U:
    json_object_set_new(o, key, json_real(pmbus_lin16u_to_double((uint16_t)w, exp5)));
    break;
  case ENC_RAW_INT:
    json_object_set_new(o, key, json_integer(w));
    break;
  default:
    __builtin_unreachable();
    break;
  }
}

static json_t *
build_read_all_json(int fd, int exp5) {
  json_t *o = json_object();

  add_read_field(o, "vin_V",       fd, PMBUS_READ_VIN,            ENC_LIN11,  0);
  add_read_field(o, "vout_V",      fd, PMBUS_READ_VOUT,           ENC_LIN16U, exp5);
  add_read_field(o, "iout_A",      fd, PMBUS_READ_IOUT,           ENC_LIN11,  0);
  add_read_field(o, "temp1_C",     fd, PMBUS_READ_TEMPERATURE_1,  ENC_LIN11,  0);
  add_read_field(o, "temp2_C",     fd, PMBUS_READ_TEMPERATURE_2,  ENC_LIN11,  0);
  add_read_field(o, "duty_pct",    fd, PMBUS_READ_DUTY_CYCLE,     ENC_LIN11,  0);
  add_read_field(o, "freq_khz_raw",fd, PMBUS_READ_FREQUENCY,      ENC_RAW_INT,0);

  return o;
}

int
cmd_read(int fd, int argc, char *const *argv, int pretty) {
  const char *what = (argc >= 1) ? argv[0] : "all";

  int exp5 = 0;
  pmbus_get_vout_mode_exp(fd, &exp5);

  if (!strcmp(what, "all")) {
    json_t *o = build_read_all_json(fd, exp5);

    json_print_or_pretty(o, pretty);

    return 0;
  }

  if (!strcmp(what, "vin")) {
    int v = pmbus_rd_word(fd, PMBUS_READ_VIN);
    if (v < 0) {
      perror("READ_VIN");
      return 1;
    }
    out_double("vin_V", pmbus_lin11_to_double((uint16_t) v), pretty);

    return 0;
  }

  if (!strcmp(what, "vout")) {
    int v = pmbus_rd_word(fd, PMBUS_READ_VOUT);
    if (v < 0) {
      perror("READ_VOUT");
      return 1;
    }
    out_double("vout_V", pmbus_lin16u_to_double((uint16_t) v, exp5), pretty);

    return 0;
  }

  if (!strcmp(what, "iout")) {
    int v = pmbus_rd_word(fd, PMBUS_READ_IOUT);
    if (v < 0) {
      perror("READ_IOUT");
      return 1;
    }
    out_double("iout_A", pmbus_lin11_to_double((uint16_t) v), pretty);

    return 0;
  }

  if (!strcmp(what, "temp1")) {
    int v = pmbus_rd_word(fd, PMBUS_READ_TEMPERATURE_1);
    if (v < 0) {
      perror("READ_TEMPERATURE_1");
      return 1;
    }
    out_double("temp1_C", pmbus_lin11_to_double((uint16_t) v), pretty);

    return 0;
  }

  if (!strcmp(what, "temp2")) {
    int v = pmbus_rd_word(fd, PMBUS_READ_TEMPERATURE_2);
    if (v < 0) {
      perror("READ_TEMPERATURE_2");
      return 1;
    }
    out_double("temp2_C", pmbus_lin11_to_double((uint16_t) v), pretty);

    return 0;
  }

  if (!strcmp(what, "duty")) {
    int v = pmbus_rd_word(fd, PMBUS_READ_DUTY_CYCLE);
    if (v < 0) {
      perror("READ_DUTY_CYCLE");
      return 1;
    }
    out_double("duty_pct", pmbus_lin11_to_double((uint16_t) v), pretty);

    return 0;
  }

  if (!strcmp(what, "freq")) {
    int v = pmbus_rd_word(fd, PMBUS_READ_FREQUENCY);
    if (v < 0) {
      perror("READ_FREQUENCY");
      return 1;
    }
    json_t *o = json_object();
    json_object_set_new(o, "freq_khz_raw", json_integer(v));
    json_print_or_pretty(o, pretty);

    return 0;
  }

  fprintf(stderr, "read [vin|vout|iout|temp1|temp2|duty|freq|all]\n");

  return 2;
}
