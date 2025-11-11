/* SPDX-License-Identifier: AGPL-3.0-or-later */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * PMBus standard (generic):
 *   - Control:            0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x15, 0x16, 0x19
 *   - VOUT/Scaling:       0x20–0x24, 0x25–0x27, 0x29–0x2A
 *   - Input/Output cfg:   0x32–0x33, 0x35–0x36, 0x38–0x39
 *   - Fault/Warn/Resp:    0x40–0x45, 0x46–0x4A, 0x4F–0x54, 0x55–0x5A, 0x5E–0x5F
 *   - Timing:             0x60–0x66
 *   - Status:             0x78–0x7E
 *   - Measurements:       0x88, 0x8B–0x8E, 0x94–0x95, 0x98
 *   - Identification:     0x99–0x9E
 *   - User data:          0xB0 (USER_DATA_00)
 *
 *   These are part of the PMBus spec. Both BMR685 and BMR456 implement the vast majority.
 *   Notable exceptions: READ_DUTY_CYCLE (0x94) and READ_FREQUENCY (0x95) are documented
 *   on BMR685; BMR456 may not expose them.
 *
 * vendor-specific (Flex “MFR_*” commands):
 *   0xC4, 0xC8, 0xD0–0xD3, 0xD5, 0xD7–0xDD, 0xDE, 0xE0–0xE3, 0xE7–0xE8, 0xEB, 0xEE,
 *   0xF1, 0xF4, 0xF8–0xF9, 0xFD–0xFE
 */

enum PMBus_opcodes : uint8_t {
  /* Control */
  PMBUS_OPERATION                 = 0x01,
  PMBUS_ON_OFF_CONFIG             = 0x02,
  PMBUS_CLEAR_FAULTS              = 0x03,
  PMBUS_WRITE_PROTECT             = 0x10,
  PMBUS_STORE_DEFAULT_ALL         = 0x11,
  PMBUS_RESTORE_DEFAULT_ALL       = 0x12,
  PMBUS_STORE_USER_ALL            = 0x15,
  PMBUS_RESTORE_USER_ALL          = 0x16,
  PMBUS_STORE_USER_CODE           = 0x17, /* not supported with BMR devices */
  PMBUS_RESTORE_USER_CODE         = 0x18, /* not supported with BMR devices */
  PMBUS_CAPABILITY                = 0x19,
  PMBUS_QUERY                     = 0x1A, /* not supported with BMR devices */
  PMBUS_SMBALERT_MASK             = 0x1B,

  /* Output programming / scaling */
  PMBUS_VOUT_MODE                 = 0x20,
  PMBUS_VOUT_COMMAND              = 0x21,
  PMBUS_VOUT_TRIM                 = 0x22,
  PMBUS_VOUT_CAL_OFFSET           = 0x23,
  PMBUS_VOUT_MAX                  = 0x24,
  PMBUS_VOUT_MARGIN_HIGH          = 0x25,
  PMBUS_VOUT_MARGIN_LOW           = 0x26,
  PMBUS_VOUT_TRANSITION_RATE      = 0x27,
  PMBUS_VOUT_SCALE_LOOP           = 0x29,
  PMBUS_VOUT_SCALE_MONITOR        = 0x2A,

  /* Misc output/input config */
  PMBUS_MAX_DUTY                  = 0x32,
  PMBUS_FREQUENCY_SWITCH          = 0x33,
  PMBUS_VIN_ON                    = 0x35,
  PMBUS_VIN_OFF                   = 0x36,
  PMBUS_INTERLEAVE                = 0x37,
  PMBUS_IOUT_CAL_GAIN             = 0x38,
  PMBUS_IOUT_CAL_OFFSET           = 0x39,

  /* Fault/Warn limits & responses */
  PMBUS_VOUT_OV_FAULT_LIMIT       = 0x40,
  PMBUS_VOUT_OV_FAULT_RESPONSE    = 0x41,
  PMBUS_VOUT_OV_WARN_LIMIT        = 0x42,
  PMBUS_VOUT_UV_WARN_LIMIT        = 0x43,
  PMBUS_VOUT_UV_FAULT_LIMIT       = 0x44,
  PMBUS_VOUT_UV_FAULT_RESPONSE    = 0x45,
  PMBUS_IOUT_OC_FAULT_LIMIT       = 0x46,
  PMBUS_IOUT_OC_FAULT_RESPONSE    = 0x47,
  PMBUS_IOUT_OC_LV_FAULT_LIMIT    = 0x48,
  PMBUS_IOUT_OC_WARN_LIMIT        = 0x4A,
  PMBUS_OT_FAULT_LIMIT            = 0x4F,
  PMBUS_OT_FAULT_RESPONSE         = 0x50,
  PMBUS_OT_WARN_LIMIT             = 0x51,
  PMBUS_UT_WARN_LIMIT             = 0x52,
  PMBUS_UT_FAULT_LIMIT            = 0x53,
  PMBUS_UT_FAULT_RESPONSE         = 0x54,
  PMBUS_VIN_OV_FAULT_LIMIT        = 0x55,
  PMBUS_VIN_OV_FAULT_RESPONSE     = 0x56,
  PMBUS_VIN_OV_WARN_LIMIT         = 0x57,
  PMBUS_VIN_UV_WARN_LIMIT         = 0x58,
  PMBUS_VIN_UV_FAULT_LIMIT        = 0x59,
  PMBUS_VIN_UV_FAULT_RESPONSE     = 0x5A,
  PMBUS_POWER_GOOD_ON             = 0x5E,
  PMBUS_POWER_GOOD_OFF            = 0x5F,

  /* Timing */
  PMBUS_TON_DELAY                 = 0x60,
  PMBUS_TON_RISE                  = 0x61,
  PMBUS_TON_MAX_FAULT_LIMIT       = 0x62,
  PMBUS_TON_MAX_FAULT_RESPONSE    = 0x63,
  PMBUS_TOFF_DELAY                = 0x64,
  PMBUS_TOFF_FALL                 = 0x65,
  PMBUS_TOFF_MAX_WARN_LIMIT       = 0x66,

  /* Status */
  PMBUS_STATUS_BYTE               = 0x78,
  PMBUS_STATUS_WORD               = 0x79,
  PMBUS_STATUS_VOUT               = 0x7A,
  PMBUS_STATUS_IOUT               = 0x7B,
  PMBUS_STATUS_INPUT              = 0x7C,
  PMBUS_STATUS_TEMPERATURE        = 0x7D,
  PMBUS_STATUS_CML                = 0x7E,
  PMBUS_OTHER                     = 0x7F, /* BMR456 */

  /* Measurements */
  PMBUS_READ_VIN                  = 0x88,
  PMBUS_READ_VOUT                 = 0x8B,
  PMBUS_READ_IOUT                 = 0x8C,
  PMBUS_READ_TEMPERATURE_1        = 0x8D,
  PMBUS_READ_TEMPERATURE_2        = 0x8E,
  PMBUS_READ_TEMPERATURE_3        = 0x8F,
  PMBUS_READ_DUTY_CYCLE           = 0x94,
  PMBUS_READ_FREQUENCY            = 0x95,

  /* Identification */
  PMBUS_PMBUS_REVISION            = 0x98,
  MFR_USER_DATA_00                = 0xB0,

  /* Identification / strings */
  MFR_ID                          = 0x99,
  MFR_MODEL                       = 0x9A,
  MFR_REVISION                    = 0x9B,
  MFR_LOCATION                    = 0x9C,
  MFR_DATE                        = 0x9D,
  MFR_SERIAL                      = 0x9E,

  /* Vendor-specific — BMR685 */
  MFR_VIN_OV_WARN_RESPONSE        = 0xC4, /* BMR685 */
  MFR_FAST_VIN_OFF_OFFSET         = 0xC8, /* BMR685 */
  MFR_PGOOD_POLARITY              = 0xD0,
  MFR_FAST_OCP_CFG                = 0xD1, /* BMR685 */
  MFR_RESPONSE_UNIT_CFG           = 0xD2, /* BMR685 */
  MFR_VIN_SCALE_MONITOR           = 0xD3,
  MFR_SNAPSHOT_CYCLES_SELECT      = 0xD5, /* BMR685 */
  MFR_GET_SNAPSHOT                = 0xD7, /* BMR685 */
  MFR_TEMP_COMPENSATION           = 0xD8, /* BMR685 */
  MFR_SET_ROM_MODE                = 0xD9, /* BMR685 */
  MFR_GET_RAMP_DATA               = 0xDB,
  MFR_SELECT_TEMPERATURE_SENSOR   = 0xDC,
  MFR_VIN_OFFSET                  = 0xDD,
  MFR_VOUT_OFFSET_MONITOR         = 0xDE,
  MFR_GET_STATUS_DATA             = 0xDF,
  MFR_SPECIAL_OPTIONS             = 0xE0, /* BMR685 */
  MFR_TEMP_OFFSET_INT             = 0xE1,
  MFR_REMOTE_TEMP_CAL             = 0xE2,
  MFR_REMOTE_CTRL                 = 0xE3,
  MFR_DEAD_BAND_DELAY             = 0xE5, /* BMR456 */
  MFR_TEMP_COEFF                  = 0xE7,
  MFR_FILTER_COEFF                = 0xE8, /* BMR685 */
  MFR_MIN_DUTY                    = 0xEB, /* BMR685 */
  MFR_OFFSET_ADDRESS              = 0xEE, /* BMR685 */
  MFR_DEBUG_BUFF                  = 0xF0, /* BMR456 */
  MFR_SETUP_PASSWORD              = 0xF1,
  MFR_DISABLE_SECURITY_ONCE       = 0xF2, /* BMR456 */
  MFR_DEAD_BAND_IOUT_THRESHOLD    = 0xF3, /* BMR456 */
  MFR_SECURITY_BIT_MASK           = 0xF4,
  MFR_PRIMARY_TURN                = 0xF5, /* BMR456 */
  MFR_SECONDARY_TURN              = 0xF6, /* BMR456 */
  MFR_ILIM_SOFTSTART              = 0xF8,
  MFR_MULTI_PIN_CONFIG            = 0xF9,
  MFR_DEAD_BAND_VIN_THRESHOLD     = 0xFA, /* BMR456 */
  MFR_DEAD_BAND_VIN_IOUT_HYS      = 0xFB, /* BMR456 */
  MFR_FIRMWARE_DATA               = 0xFD, /* BMR685 */
  MFR_RESTART                     = 0xFE,
};

int pmbus_open(const char *dev, int addr7);
void pmbus_close(int fd);
int pmbus_rd_byte(int fd, uint8_t cmd);
int pmbus_rd_word(int fd, uint8_t cmd);
int pmbus_rd_block(int fd, uint8_t cmd, uint8_t * buf, int max);
int pmbus_wr_byte(int fd, uint8_t cmd, uint8_t val);
int pmbus_wr_word(int fd, uint8_t cmd, uint16_t val);
int pmbus_wr_block(int fd, uint8_t cmd, const uint8_t * buf, int len);
int pmbus_send_byte(int fd, uint8_t cmd);

/* returns 0 if linear mode */
int pmbus_get_vout_mode_exp(int fd, int *exp_out);
double pmbus_lin11_to_double(uint16_t raw);
double pmbus_lin16u_to_double(uint16_t raw, int exp5);

uint16_t le16(const uint8_t * p);
uint32_t le32(const uint8_t * p);

int parse_u16(const char *s, uint16_t *out);
