sources = [
  'main.c',
  'pmbus_io.c',
  'decoders.c',
  'mfr_snapshot.c',
  'mfr_multipin.c',
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

