/* SPDX-License-Identifier: AGPL-3.0-or-later */

#pragma once

#include <jansson.h>
#include <stdint.h>

void json_print_or_pretty(json_t * o, int pretty);
int json_add_hex_ascii(json_t *dst, const char *key, const void *buf, size_t n);
int json_add_len_and_hex(json_t *dst, const char *key, const void *buf, size_t n);

void rd_block_string(int fd, uint8_t cmd, const char *key, json_t *root);
