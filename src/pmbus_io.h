#pragma once

#include <stdint.h>
#include <stdbool.h>

enum PMBus_opcodes : uint8_t {
  PMBUS_CLEAR_FAULTS = 0x03,
  PMBUS_STORE_DEFAULT_ALL = 0x11,
  PMBUS_RESTORE_DEFAULT_ALL = 0x12,
  PMBUS_STORE_USER_ALL = 0x15,
  PMBUS_RESTORE_USER_ALL = 0x16,
  PMBUS_VOUT_MODE = 0x20,
  PMBUS_READ_VIN = 0x88,
  PMBUS_READ_VOUT = 0x8B,
  PMBUS_READ_IOUT = 0x8C,
  PMBUS_READ_TEMPERATURE_1 = 0x8D,
  PMBUS_READ_TEMPERATURE_2 = 0x8E,
  PMBUS_READ_DUTY_CYCLE = 0x94,
  PMBUS_READ_FREQUENCY = 0x95,
  PMBUS_STATUS_BYTE = 0x78,
  PMBUS_STATUS_WORD = 0x79,
  PMBUS_STATUS_VOUT = 0x7A,
  PMBUS_STATUS_IOUT = 0x7B,
  PMBUS_STATUS_INPUT = 0x7C,
  PMBUS_STATUS_TEMPERATURE = 0x7D,
  PMBUS_STATUS_CML = 0x7E,

  MFR_USER_DATA_00 = 0xB0,
  MFR_SNAPSHOT_CYCLES_SELECT = 0xD5,
  MFR_GET_SNAPSHOT = 0xD7,
  MFR_MULTI_PIN_CONFIG = 0xF9,
  MFR_FIRMWARE_DATA = 0xFD,
  MFR_RESTART = 0xFE,
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
