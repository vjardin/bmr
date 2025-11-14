/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include <stdio.h>
#include <string.h>

int
cmd_save(int fd) {
  uint8_t b[64];

  // For BMR456 STORE and RESTORE is not based on send byte but on a write byte with a dummy value
  // Product version is read to know how to execute the command
  pmbus_rd_block(fd, MFR_MODEL, b, (int)sizeof b);

  if (strncmp((char *)b, "BMR456", (size_t)6))
    pmbus_send_byte(fd, PMBUS_STORE_USER_ALL);
  else
    pmbus_wr_byte(fd, PMBUS_STORE_USER_ALL, 0x01);

  puts("OK");

  return 0;
}

int
cmd_restore(int fd, int argc, char *const *argv) {
  uint8_t b[64];
  bool isDefault = false;

  if ((argc) && !strcmp(argv[0], "default"))
    isDefault = true;


  // For BMR456 STORE and RESTORE is not based on send byte but on a write byte with a dummy value
  // Product version is read to know how to execute the command
  pmbus_rd_block(fd, MFR_MODEL, b, (int)sizeof b);

  if (isDefault) {
    if (strncmp((char *)b, "BMR456", (size_t)6))
      pmbus_send_byte(fd, PMBUS_RESTORE_DEFAULT_ALL);
    else
      pmbus_wr_byte(fd, PMBUS_RESTORE_DEFAULT_ALL, 0x01);
  } else {
    if (strncmp((char *)b, "BMR456", (size_t)6))
      pmbus_send_byte(fd, PMBUS_RESTORE_USER_ALL);
    else
      pmbus_wr_byte(fd, PMBUS_RESTORE_USER_ALL, 0x01);
  }

  puts("OK");

  return 0;
}
