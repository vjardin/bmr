/* SPDX-License-Identifier: AGPL-3.0-or-later */

#pragma once

#include <stdint.h>
#include <math.h> /* ldexp: no need of libm */

/* TODO: align with PMBus-Specification-Rev-1-3-1-Part-II-20150313.pdf */

/* Round-to-nearest and saturate into [0, 65535] for non-negative doubles. */
static inline uint16_t
u16_round_sat_pos(double x) {
  if (isnan(x))
    return 0;

  if (x >= 65535.0)
    return 65535;

  return (uint16_t)(x + 0.5);
}

/* Linear16-Unsigned: units = y * 2^N */
static inline double
lin16u_to_units(uint16_t y, int expN) {
  return ldexp((double)y, expN);
}

/* lin16u = round(units * 2^{-N}) */
static inline uint16_t units_to_lin16u(double units, int expN) {
  return u16_round_sat_pos(ldexp(units, -expN));
}
