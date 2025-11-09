/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "util_json.h"
#include "mfr_snapshot.h"
#include "mfr_multipin.h"
#include "mfr_id.h"
#include "timing_cmd.h"
#include "read_cmd.h"
#include "onoff_cmd.h"
#include "operation_cmd.h"
#include "status_cmd.h"
#include "vout_cmd.h"
#include "mfr_fwdata.h"
#include "mfr_restart.h"
#include "mfr_user_data.h"

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

"Usage: %s --bus DEV --addr 0xHH [-P/--pretty-off] <command> [args]\n"
"\n"
"Commands:\n"
"  read [vin|vout|iout|temp1|temp2|duty|freq|all]\n"
"  status\n"
"  snapshot [--cycle 0..19] [--decode]\n"
"  mfr-multi-pin get|set [--mode MODE] [--pg pushpull|highz] [--pg-enable 0|1] [--sec-rc-pull 0|1]\n"
"  id\n"
"  fwdata\n"
"  restart\n"
"  user-data get|set [--hex XX..|--ascii STR] [--store|--restore]\n"
"  timing get|set [--profile safe|sequenced|fast|prebias]\n"
"                 [--ton-delay MS] [--ton-rise MS] [--ton-max-fault MS]\n"
"                 [--toff-delay MS] [--toff-fall MS] [--toff-max-warn MS]\n"
"                 [--fault-byte 0xHH]\n"
"                 [--fault-response disable-retry|disable-until-cleared|ignore]\n"
"                 [--retries 0..7] [--delay-units 0..7]\n"
"  onoff get|set [--powerup always|controlled] [--source none|operation|pin|both]\n"
"                [--en-active high|low] [--off soft|immediate] [--raw 0xHH]\n"
"  operation get|set [--on|--off] [--margin normal|low|high] [--raw 0xHH]\n"
"  vout get|set [--command V] [--mhigh V] [--mlow V]\n"
"               [--set-all NOM --margin-pct +/-PCT]\n"
"\n"
"Default:\n"
"  i2c DEV=%s addr=0x%02x\n"

, p
, opt_bus
, opt_addr
  );

}

int
main(int argc, char *const *argv) {
  static const char* Lopt = "+b:a:Ph";
  static const struct option L[] = {
      { "bus", required_argument, NULL, 'b' }
    , { "addr", required_argument, NULL, 'a' }
    , { "pretty-off", no_argument, NULL, 'P' }
    , { "help", no_argument, NULL, 'h' }
    , { }
  };

  int c;
  while ((c = getopt_long(argc, argv, Lopt, L, NULL)) != -1) {
    switch(c) {
      case 'b':
        opt_bus = optarg;
        break;
      case 'a':
        opt_addr = (int)strtol(optarg, NULL, 0);
        break;
      case 'P':
        opt_pretty = 0;
        break;
      case 'h':
      default:
        usage(argv[0]);
        return EXIT_SUCCESS;
        break;
    }
  }

  if (optind >= argc) {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  const char *cmd = argv[optind++];
  optind = 0; /* reset */

  int fd = pmbus_open(opt_bus, opt_addr);
  if (fd < 0) {
    perror("open bus");
    return EXIT_FAILURE;
  }

  int rc = EXIT_SUCCESS;
  int sub_argc = argc - optind;
  char * const *sub_argv = &argv[optind];

  if (!strcmp(cmd, "read")) {
    rc = cmd_read(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "status")) {
    rc = cmd_status(fd, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "snapshot")) {
    rc = cmd_snapshot(fd, sub_argc, sub_argv);
    goto fini;
  }

  if (!strcmp(cmd, "mfr-multi-pin")) {
    rc = cmd_multipin(fd, sub_argc, sub_argv);
    goto fini;
  }

  if (!strcmp(cmd, "id")) {
    rc = cmd_mfr_id(fd, sub_argc, sub_argv);
    goto fini;
  }

  if (!strcmp(cmd, "fwdata")) {
    rc = cmd_fwdata(fd, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "restart")) {
    rc = cmd_restart(fd);
    goto fini;
  }

  if (!strcmp(cmd, "user-data")) {
    rc = cmd_user_data(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "timing")) {
    rc = cmd_timing(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "onoff")) {
    rc = cmd_onoff(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "operation")) {
    rc = cmd_operation(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "vout")) {
    rc = cmd_vout(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  usage(argv[0]);
  rc = EXIT_FAILURE;

fini:
  pmbus_close(fd);

  return rc;
}
