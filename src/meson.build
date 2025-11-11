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
  'mfr_hrr.c',
  'mfr_ramp_data.c',
  'mfr_status_data.c',
  'mfr_addr_offset.c',
  'timing_cmd.c',
  'read_cmd.c',
  'status_cmd.c',
  'onoff_cmd.c',
  'operation_cmd.c',
  'vout_cmd.c',
  'interleave_cmd.c',
  'vin_cmd.c',
  'pgood_cmd.c',
  'freq_cmd.c',
  'salert_cmd.c',
  'write_protect_cmd.c',
  'capability_cmd.c',
  'fault_cmd.c',
  'temp_cmd.c',
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

