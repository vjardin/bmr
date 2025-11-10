/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include <stdio.h>

int
cmd_restart(int fd) {
  const char *s = "ERIC";

  if (pmbus_wr_block(fd, MFR_RESTART, (const uint8_t *) s, 4) < 0) {
    perror("MFR_RESTART");
    return 1;
  }
  puts("OK");

  return 0;
}
