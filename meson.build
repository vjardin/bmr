# SPDX-License-Identifier: AGPL-3.0-or-later

project('bmr', 'c',
  version: '0.1.0',
  default_options: [
    'warning_level=3',
    'c_std=c2x'
  ]
)

fully_static = get_option('fully_static')

cc = meson.get_compiler('c')

jansson_dep = dependency('jansson', required: true)
# XXX
libi2c_dep = cc.find_library('i2c', required: true)

subdir('src')
