#pragma once

#include <stdint.h>
#include <jansson.h>

json_t *decode_status_byte(uint8_t v);
json_t *decode_status_word(uint16_t w);
json_t *decode_status_vout(uint8_t v);
json_t *decode_status_iout(uint8_t v);
json_t *decode_status_input(uint8_t v);
json_t *decode_status_temperature(uint8_t v);
json_t *decode_status_cml(uint8_t v);
