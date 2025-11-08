#include "pmbus_io.h"
#include "decoders.h"
#include "mfr_snapshot.h"
#include "mfr_multipin.h"
#include "util_json.h"
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

static const char *opt_bus = "/dev/i2c-1";
static int opt_addr = 0x40;
static int opt_pretty = 1;

static void
usage(const char *p) {
  fprintf(stderr,
          "Usage: %s --bus DEV --addr 0xHH <command> [args]\n\n"
          "Commands:\n"
          "  read [vin|vout|iout|temp1|temp2|duty|freq|all]\n"
          "  status\n"
          "  snapshot [--cycle 0..19] [--decode]\n"
          "  mfr-multi-pin get|set [--mode MODE] [--pg pushpull|highz] [--pg-enable 0|1] [--sec-rc-pull 0|1]\n"
          "  fwdata\n" "  restart\n" "  user-data get|set [--hex XX..|--ascii STR] [--store|--restore]\n", p);
}

static void
out_double(const char *k, double v) {
  json_t *o = json_object();
  json_object_set_new(o, k, json_real(v));
  json_print_or_pretty(o, opt_pretty);
}

static int
cmd_status_all(int fd) {
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

  json_print_or_pretty(o, opt_pretty);

  return 0;
}

static int
cmd_fw(int fd) {
  uint8_t b[32];

  int n = pmbus_rd_block(fd, MFR_FIRMWARE_DATA, b, sizeof b);
  if (n < 0) {
    perror("MFR_FIRMWARE_DATA");
    return 1;
  }

  json_t *o = json_object();
  json_object_set_new(o, "len", json_integer(n));

  char *hex = malloc(n * 2 + 1);
  for (int i = 0; i < n; i++)
    sprintf(hex + 2 * i, "%02X", b[i]);
  hex[n * 2] = '\0';
  json_object_set_new(o, "hex", json_string(hex));
  free(hex);
  // Extract printable runs >= 3
  json_t *runs = json_array();

  int run = 0, start = 0;
  for (int i = 0; i < n; i++) {
    if (b[i] >= 32 && b[i] <= 126) {
      if (run == 0)
        start = i;
      run++;
    } else {
      if (run >= 3)
        json_array_append_new(runs, json_stringn((char *) &b[start], run));
      run = 0;
  }}
  if (run >= 3)
    json_array_append_new(runs, json_stringn((char *) &b[start], run));
  json_object_set_new(o, "ascii_runs", runs);
  json_print_or_pretty(o, opt_pretty);

  return 0;
}

static int
cmd_restart_do(int fd) {
  const char *s = "00000000";

  if (pmbus_wr_block(fd, MFR_RESTART, (const uint8_t *) s, 8) < 0) {
    perror("MFR_RESTART");
    return 1;
  }
  puts("OK");

  return 0;
}

static int
cmd_user(int fd, int argc, char * const * argv) {
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
    char *hex = malloc(n * 2 + 1);
    for (int i = 0; i < n; i++)
      sprintf(hex + 2 * i, "%02X", buf[i]);
    hex[n * 2] = '\0';
    json_object_set_new(o, "hex", json_string(hex));
    free(hex);
    json_print_or_pretty(o, opt_pretty);

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
      } n = (int) (L / 2);
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

int
main(int argc, char * const *argv) {
  static const struct option L[] = {
      { "bus", required_argument, NULL, 'b' }
    , { "addr", required_argument, NULL, 'a' }
    , { "pretty", no_argument, NULL, 'p' }
    , { "help", no_argument, NULL, 'h' }
    , { }
  };

  int c;
  while ((c = getopt_long(argc, argv, "b:a:ph", L, NULL)) != -1) {
    if (c == 'b')
      opt_bus = optarg;
    else if (c == 'a')
      opt_addr = (int) strtol(optarg, NULL, 0);
    else if (c == 'p')
      opt_pretty = 1;
    else if (c == 'h') {
      usage(argv[0]);
      return EXIT_SUCCESS;
    }
  }
  if (optind >= argc) {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  const char *cmd = argv[optind++];
  int fd = pmbus_open(opt_bus, opt_addr);
  if (fd < 0) {
    perror("open bus");
    return EXIT_FAILURE;
  }

  int rc = EXIT_SUCCESS;
  /* finalize using goto fini in order to close the i2c/pmbus */

  if (!strcmp(cmd, "read")) {
    const char *what = (optind < argc) ? argv[optind] : "all";
    int exp5 = 0;
    pmbus_get_vout_mode_exp(fd, &exp5);

    if (!strcmp(what, "all")) {
      int vin = pmbus_rd_word(fd, PMBUS_READ_VIN);
      int vout = pmbus_rd_word(fd, PMBUS_READ_VOUT);
      int iout = pmbus_rd_word(fd, PMBUS_READ_IOUT);
      int t1 = pmbus_rd_word(fd, PMBUS_READ_TEMPERATURE_1);
      int t2 = pmbus_rd_word(fd, PMBUS_READ_TEMPERATURE_2);
      int duty = pmbus_rd_word(fd, PMBUS_READ_DUTY_CYCLE);
      int freq = pmbus_rd_word(fd, PMBUS_READ_FREQUENCY);

      json_t *o = json_object();
      if (vin >= 0)
        json_object_set_new(o, "vin_V", json_real(pmbus_lin11_to_double((uint16_t) vin)));
      if (vout >= 0)
        json_object_set_new(o, "vout_V", json_real(pmbus_lin16u_to_double((uint16_t) vout, exp5)));
      if (iout >= 0)
        json_object_set_new(o, "iout_A", json_real(pmbus_lin11_to_double((uint16_t) iout)));
      if (t1 >= 0)
        json_object_set_new(o, "temp1_C", json_real(pmbus_lin11_to_double((uint16_t) t1)));
      if (t2 >= 0)
        json_object_set_new(o, "temp2_C", json_real(pmbus_lin11_to_double((uint16_t) t2)));
      if (duty >= 0)
        json_object_set_new(o, "duty_pct", json_real(pmbus_lin11_to_double((uint16_t) duty)));
      if (freq >= 0)
        json_object_set_new(o, "freq_khz_raw", json_integer(freq));
      json_print_or_pretty(o, opt_pretty);

      goto fini;
    }

    if (!strcmp(what, "vin")) {
      int v = pmbus_rd_word(fd, PMBUS_READ_VIN);
      if (v < 0) {
        perror("READ_VIN");
        rc = EXIT_FAILURE;
        goto fini;
      }
      out_double("vin_V", pmbus_lin11_to_double((uint16_t) v));

      goto fini;
    }

    if (!strcmp(what, "vout")) {
      int v = pmbus_rd_word(fd, PMBUS_READ_VOUT);
      if (v < 0) {
        perror("READ_VOUT");
        rc = EXIT_FAILURE;
        goto fini;
      }
      out_double("vout_V", pmbus_lin16u_to_double((uint16_t) v, exp5));

      goto fini;
    }

    if (!strcmp(what, "iout")) {
      int v = pmbus_rd_word(fd, PMBUS_READ_IOUT);
      if (v < 0) {
        perror("READ_IOUT");
        rc = EXIT_FAILURE;
        goto fini;
      }
      out_double("iout_A", pmbus_lin11_to_double((uint16_t) v));

      goto fini;
    }

    if (!strcmp(what, "temp1")) {
      int v = pmbus_rd_word(fd, PMBUS_READ_TEMPERATURE_1);
      if (v < 0) {
        perror("READ_TEMPERATURE_1");
        rc = EXIT_FAILURE;
        goto fini;
      }
      out_double("temp1_C", pmbus_lin11_to_double((uint16_t) v));

      goto fini;
    }

    if (!strcmp(what, "temp2")) {
      int v = pmbus_rd_word(fd, PMBUS_READ_TEMPERATURE_2);
      if (v < 0) {
        perror("READ_TEMPERATURE_2");
        rc = EXIT_FAILURE;
        goto fini;
      }
      out_double("temp2_C", pmbus_lin11_to_double((uint16_t) v));

      goto fini;
    }

    if (!strcmp(what, "duty")) {
      int v = pmbus_rd_word(fd, PMBUS_READ_DUTY_CYCLE);
      if (v < 0) {
        perror("READ_DUTY_CYCLE");
        rc = EXIT_FAILURE;
        goto fini;
      }
      out_double("duty_pct", pmbus_lin11_to_double((uint16_t) v));

      goto fini;
    }

    if (!strcmp(what, "freq")) {
      int v = pmbus_rd_word(fd, PMBUS_READ_FREQUENCY);
      if (v < 0) {
        perror("READ_FREQUENCY");
        rc = EXIT_FAILURE;
        goto fini;
      }
      json_t *o = json_object();
      json_object_set_new(o, "freq_khz_raw", json_integer(v));
      json_print_or_pretty(o, opt_pretty);

      goto fini;
    }

    /* unknown what */
    usage(argv[0]);
    rc = EXIT_FAILURE;

    goto fini;
  } /* cmd read */

  if (!strcmp(cmd, "status")) {
    rc = cmd_status_all(fd);
    goto fini;
  }

  if (!strcmp(cmd, "snapshot")) {
    rc = cmd_snapshot(fd, argc - optind, &argv[optind]);
    goto fini;
  }

  if (!strcmp(cmd, "mfr-multi-pin")) {
    rc = cmd_multipin(fd, argc - optind, &argv[optind]);
    goto fini;
  }

  if (!strcmp(cmd, "fwdata")) {
    rc = cmd_fw(fd);
    goto fini;
  }

  if (!strcmp(cmd, "restart")) {
    rc = cmd_restart_do(fd);
    goto fini;
  }

  if (!strcmp(cmd, "user-data")) {
    rc = cmd_user(fd, argc - optind, &argv[optind]);
    goto fini;
  }

  usage(argv[0]);
  rc = EXIT_FAILURE;

fini:
  pmbus_close(fd);
  return rc;
}
