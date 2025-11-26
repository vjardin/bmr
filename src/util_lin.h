/* SPDX-License-Identifier: AGPL-3.0-or-later */

#pragma once

#include "pmbus_io.h"

#include <stdint.h>
#include <math.h> /* ldexp: no need of libm */

/* Round-to-nearest and saturate into [0, 65535] for non-negative doubles. */
static inline uint16_t
u16_round_sat_pos(double x) {
  if ((x <= D(0.0f)) || isnan(x))
    return 0u;

  if (x >= D(65535.0f))
    return UINT16_MAX;

  return (uint16_t)(x + D(0.5f));
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
