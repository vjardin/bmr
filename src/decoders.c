#include "decoders.h"

#include "status.h"
#define EMIT_STATUS_BYTE(name, bitno) JSON_SET_BIT(o, name, b, (uint8_t)BIT(bitno));
#define EMIT_STATUS_WORD(name, bitno) JSON_SET_BIT(o, name, w, (uint16_t)BIT(bitno));

json_t *
decode_status_byte(uint8_t b) {
  json_t *o = json_object();

  STATUS_BYTE_FIELDS(EMIT_STATUS_BYTE);

  return o;
}

json_t *
decode_status_word(uint16_t w) {
  json_t *o = json_object();

  STATUS_WORD_FIELDS(EMIT_STATUS_WORD);

  return o;
}

json_t *
decode_status_vout(uint8_t b) {
  json_t *o = json_object();

  STATUS_VOUT_FIELDS(EMIT_STATUS_BYTE);

  return o;
}

json_t *
decode_status_iout(uint8_t b) {
  json_t *o = json_object();

  STATUS_IOUT_FIELDS(EMIT_STATUS_BYTE);

  return o;
}

json_t *
decode_status_input(uint8_t b) {
  json_t *o = json_object();

  STATUS_INPUT_FIELDS(EMIT_STATUS_BYTE);

  return o;
}

json_t *
decode_status_temperature(uint8_t b) {
  json_t *o = json_object();

  STATUS_TEMPERATURE_FIELDS(EMIT_STATUS_BYTE);

  return o;
}

json_t *
decode_status_cml(uint8_t b) {
  json_t *o = json_object();

  STATUS_CML_FIELDS(EMIT_STATUS_BYTE);

  return o;
}
