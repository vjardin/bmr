/* SPDX-License-Identifier: AGPL-3.0-or-later */

#pragma once

#include <jansson.h>
#include <stdint.h>

#define BIT(N) ( (uint32_t)1u << ((N)) )

#define JSON_SET_BIT(obj, label, value, bitmask) \
  json_object_set_new((obj), (label), json_boolean(((value) & (bitmask)) != 0))

/* 0x78 */
#define STATUS_BYTE_FIELDS(X) \
  X("OFF",         6) \
  X("VOUT_OV",     5) \
  X("IOUT_OC",     4) \
  X("VIN_UV",      3) \
  X("TEMPERATURE", 2) \
  X("CML",         1) \
  X("OTHER",       0)

/* 0x79 */
#define STATUS_WORD_FIELDS(X) \
  X("VOUT",       15) \
  X("IOUT_POUT",  14) \
  X("INPUT",      13) \
  X("POWER_GOOD", 11) \
  X("OFF",         6) \
  X("VOUT_OV",     5) \
  X("IOUT_OC",     4) \
  X("VIN_UV",      3) \
  X("TEMPERATURE", 2) \
  X("CML",         1) \
  X("OTHER",       0)

/* 0x7A */
#define STATUS_VOUT_FIELDS(X) \
  X("VOUT_OV_FAULT",  7) \
  X("VOUT_OV_WARN",   6) \
  X("VOUT_UV_WARN",   5) \
  X("VOUT_UV_FAULT",  4) \
  X("VOUT_MAX_WARN",  3) \
  X("TON_MAX_FAULT",  2) \
  X("TOFF_MAX_WARN",  1) \
  X("OTHER",          0)

/* 0x7B */
#define STATUS_IOUT_FIELDS(X) \
  X("IOUT_OC_FAULT",     7) \
  X("IOUT_OC_LV_FAULT",  6) \
  X("IOUT_OC_WARN",      5) \
  X("IOUT_UC_FAULT",     4)

/* 0x7C */
#define STATUS_INPUT_FIELDS(X) \
  X("VIN_OV_FAULT",   7) \
  X("VIN_OV_WARN",    6) \
  X("VIN_UV_WARN",    5) \
  X("VIN_UV_FAULT",   4) \
  X("INSUFFICIENT_VIN", 3)

/* 0x7D */
#define STATUS_TEMPERATURE_FIELDS(X) \
  X("OT_FAULT", 7) \
  X("OT_WARN",  6) \
  X("UT_WARN",  5) \
  X("UT_FAULT", 4)

/* 0x7E */
#define STATUS_CML_FIELDS(X) \
  X("INVALID_COMMAND",   7) \
  X("INVALID_DATA",      6) \
  X("PEC_FAILED",        5) \
  X("MEMORY_FAULT",      4) \
  X("OTHER_COMM_FAULT",  1) \
  X("MEMORY_LOGIC_FAULT",0)
