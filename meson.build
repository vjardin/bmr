# SPDX-License-Identifier: AGPL-3.0-or-later

project('bmr', 'c',
  version: '0.1.0',
  default_options: [
    'warning_level=3',
    'c_std=c2x'
  ]
)

pkgconf = find_program(['pkg-config', 'pkgconf'], required: false)
if not pkgconf.found()
  error('pkg-config (or pkgconf) is required. Install it (Ubuntu: `sudo apt install pkg-config`).')
endif
pkgconf_ver = run_command(pkgconf, '--version', check: true).stdout().strip()
message('pkg-config found, version: ' + pkgconf_ver)

fully_static = get_option('fully_static')

cc = meson.get_compiler('c')

jansson_dep = dependency('jansson', required: true, static: fully_static)
# XXX
libi2c_dep = cc.find_library('i2c', required: true, static: fully_static)

subdir('src')
