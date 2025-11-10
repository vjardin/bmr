/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "util_json.h"
#include <jansson.h>
#include <stdint.h>

static void
rd_pmbus_revision (int fd, json_t *root)
{
  int v = pmbus_rd_byte (fd, PMBUS_PMBUS_REVISION);
  if (v < 0)
    return;

  uint8_t raw = (uint8_t) v;
  int major = (raw >> 4) & 0x0F;
  int minor = raw & 0x0F;

  json_t *o = json_object ();
  json_object_set_new (o, "raw", json_integer (raw));
  json_object_set_new (o, "major", json_integer (major));
  json_object_set_new (o, "minor", json_integer (minor));
  json_object_set_new (root, "PMBUS_REVISION", o);
}

int
cmd_mfr_id (int fd, int argc, char *const *argv, int pretty)
{
  (void) argc;
  (void) argv;

  json_t *root = json_object ();

  rd_pmbus_revision (fd, root);

  rd_block_string (fd, MFR_ID, "MFR_ID", root);
  rd_block_string (fd, MFR_MODEL, "MFR_MODEL", root);
  rd_block_string (fd, MFR_REVISION, "MFR_REVISION", root);
  rd_block_string (fd, MFR_LOCATION, "MFR_LOCATION", root);
  rd_block_string (fd, MFR_DATE, "MFR_DATE", root);
  rd_block_string (fd, MFR_SERIAL, "MFR_SERIAL", root);

  json_print_or_pretty (root, pretty);

  return 0;
}
