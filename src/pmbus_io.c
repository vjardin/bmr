/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include "pmbus_io.h"
#include "util_lin.h"

#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

int
pmbus_open(const char *dev, int addr7) {
  int fd = open(dev, O_RDWR);
  if (fd < 0)
    return -1;

  if (ioctl(fd, I2C_SLAVE, addr7) < 0) {
    int e = errno;
    close(fd);
    errno = e;
    return -1;
  }

  return fd;
}

void
pmbus_close(int fd) {
  if (fd >= 0)
    close(fd);
}

int
pmbus_rd_byte(int fd, uint8_t cmd) {
  return i2c_smbus_read_byte_data(fd, cmd);
}

int
pmbus_rd_word(int fd, uint8_t cmd) {
  return i2c_smbus_read_word_data(fd, cmd);
}

int
pmbus_rd_block(int fd, uint8_t cmd, uint8_t *buf, int max) {
  int n = i2c_smbus_read_block_data(fd, cmd, buf);
  if (n > max)
    n = max;
  return n;
}

int
pmbus_wr_byte(int fd, uint8_t cmd, uint8_t val) {
  return i2c_smbus_write_byte_data(fd, cmd, val);
}

int
pmbus_wr_word(int fd, uint8_t cmd, uint16_t val) {
  return i2c_smbus_write_word_data(fd, cmd, val);
}

int
pmbus_wr_block(int fd, uint8_t cmd, const uint8_t *buf, uint8_t len) {
  return i2c_smbus_write_block_data(fd, cmd, len, buf);
}

int
pmbus_send_byte(int fd, uint8_t cmd) {
  return i2c_smbus_write_byte(fd, cmd);
}

/* see PMBus-Specification-Rev-1-3-1-Part-II-20150313.pdf, section 8.3 */
int
pmbus_get_vout_mode_exp(int fd, int *exp_out) {
  int v = pmbus_rd_byte(fd, PMBUS_VOUT_MODE);

  if (v < 0)
    return v;

  uint8_t b = (uint8_t) v;
  int mode = (b >> 5) & 7;
  int8_t e = (int8_t) (b & 0x1F);

  if (e & 0x10)
    e |= ~0x1F;

  *exp_out = e;

  return (mode == 0) ? 0 : 1;
}

double
pmbus_lin11_to_double(uint16_t raw) {
  int16_t s = (int16_t) raw;
  int8_t exp = (int8_t) (s >> 11);
  int16_t mant = s & 0x7FF;

  if (mant & 0x400)
    mant |= ~0x7FF;

  /* value = mantissa * 2^exp */
  return ldexp((double)mant, exp);
}

double
pmbus_lin16u_to_double(uint16_t raw, int exp5) {
  return lin16u_to_units(raw, exp5);
}

uint16_t
le16(const uint8_t *p) {
  return (uint16_t)(
        (uint16_t)p[0]
      | ((uint16_t)p[1] << 8)
      );

}

uint32_t
le32(const uint8_t *p) {
  return (uint32_t)  p[0]
       | ((uint32_t) p[1] << 8)
       | ((uint32_t) p[2] << 16)
       | ((uint32_t) p[3] << 24)
       ;
}

int
parse_u16(const char *s, uint16_t *out) {
  char *end = NULL;

  errno = 0;
  long v = strtol(s, &end, 0);
  if (errno || !end || *end)
    return -1;

  if (v < 0 || v > 0xFFFF)
    return -1;

  *out = (uint16_t)v;

  return 0;
}
