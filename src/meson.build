# SPDX-License-Identifier: AGPL-3.0-or-later

sources = [
  'main.c',
  'pmbus_io.c',
  'decoders.c',
  'mfr_snapshot.c',
  'mfr_multipin.c',
  'mfr_id.c',
  'mfr_fwdata.c',
  'mfr_restart.c',
  'mfr_user_data.c',
  'timing_cmd.c',
  'read_cmd.c',
  'status_cmd.c',
  'util_json.c',
]

incs = include_directories('.')

executable('bmr',
  sources,
  include_directories: incs,
  dependencies: [jansson_dep, libi2c_dep],
  install: true,
  link_args: fully_static ? ['-static'] : [],
)

