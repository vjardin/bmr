/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "util_json.h"
#include "mfr_snapshot.h"
#include "mfr_multipin.h"
#include "mfr_id.h"
#include "mfr_hrr.h"
#include "mfr_ramp_data.h"
#include "mfr_addr_offset.h"
#include "mfr_status_data.h"
#include "timing_cmd.h"
#include "read_cmd.h"
#include "onoff_cmd.h"
#include "operation_cmd.h"
#include "fault_cmd.h"
#include "temp_cmd.h"
#include "status_cmd.h"
#include "vout_cmd.h"
#include "interleave_cmd.h"
#include "vin_cmd.h"
#include "pgood_cmd.h"
#include "freq_cmd.h"
#include "salert_cmd.h"
#include "write_protect_cmd.h"
#include "capability_cmd.h"
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
"  fault get [all|temp|vin|vout|tonmax|iout]\n"
"  fault temp set [--ot-delay 16s|32s|2^n] [--ot-mode disable-retry] [--ot-retries cont]\n"
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
"  capability get\n"
"  capability check [--need-pec on|off] [--min-speed 100|400] [--need-alert on|off] [--strict]\n"
"  interleave get|set [--set 0xNN] [--phases 1..16 --index 0..15]\n"
"  hrr get|set [--pec on|off] [--hrr on|off] [--dls linear|nonlinear]\n"
"              [--artdlc on|off] [--dbv on|off] [--raw 0xNN]\n"
"  vin get [--exp5 N] [--raw]\n"
"  vin set [--on V] [--off V] [--exp5 N] | [--on-raw 0xNNNN] [--off-raw 0xNNNN]\n"
"  pgood get [--exp5 N] [--raw]\n"
"  pgood set [--on V] [--off V] [--exp5 N] | [--on-raw 0xNNNN] [--off-raw 0xNNNN]\n"
"  freq get|set --raw 0xNNNN\n"
"  salert get|set --raw 0xNN\n"
"  addr-offset get|set --raw 0xNN\n"
"  ramp-data\n"
"  status-data\n"
"  write-protect get|set [--none|--ctrl|--nvm|--all] | --raw 0xNN\n"
"  temp get  [all|ot|ut|warn]\n"
"  temp set  [--ot-fault <C>] [--ut-fault <C>] [--ot-warn <C>] [--ut-warn <C>]\n"
"  temp read [all|t1|t2|t3]\n"
"\n"
"Hints:\n"
"  * Use '<command> help' where available (e.g., 'hrr help', 'capability help', 'fault help') for detailed docs.\n"
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

  int fd = pmbus_open(opt_bus, opt_addr);
  if (fd < 0) {
    perror("open bus");
    return EXIT_FAILURE;
  }

  int rc = EXIT_SUCCESS;
  int sub_argc = argc - optind;
  char * const *sub_argv = &argv[optind];

  optind = 0; /* reset */

  if (!strcmp(cmd, "read")) {
    rc = cmd_read(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "status")) {
    rc = cmd_status(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "snapshot")) {
    rc = cmd_snapshot(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "mfr-multi-pin")) {
    rc = cmd_multipin(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "id")) {
    rc = cmd_mfr_id(fd, sub_argc, sub_argv, opt_pretty);
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

  if (!strcmp(cmd, "interleave")) {
    rc = cmd_interleave(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "hrr")) {
    rc = cmd_hrr(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "vin")) {
    rc = cmd_vin(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "pgood")) {
    rc = cmd_pgood(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "freq")) {
    rc = cmd_freq(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "salert")) {
    rc = cmd_salert(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "addr-offset")) {
    rc = cmd_addr_offset(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "ramp-data")) {
    rc = cmd_ramp_data(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "status-data")) {
    rc = cmd_status_data(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "write-protect")) {
    rc = cmd_write_protect(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "fault")) {
    rc = cmd_fault(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "temp")) {
    rc = cmd_temp(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  if (!strcmp(cmd, "capability")) {
    rc = cmd_capability(fd, sub_argc, sub_argv, opt_pretty);
    goto fini;
  }

  usage(argv[0]);
  rc = EXIT_FAILURE;

fini:
  pmbus_close(fd);

  return rc;
}
