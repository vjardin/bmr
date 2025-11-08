#include "decoders.h"

static json_t *
bits8(uint8_t v) {

  json_t *o = json_object();
  for (int i = 7; i >= 0; i--) {
    char k[8];
    snprintf(k, sizeof k, "b%d", i);
    json_object_set_new(o, k, json_boolean(!!(v & (1 << i))));
  }

  return o;
}

json_t *
decode_status_byte(uint8_t v) {
  json_t *o = bits8(v);

  json_object_set_new(o, "BUSY", json_boolean(!!(v & 0x80)));
  json_object_set_new(o, "OFF", json_boolean(!!(v & 0x40)));
  json_object_set_new(o, "VOUT_OV", json_boolean(!!(v & 0x20)));
  json_object_set_new(o, "IOUT_OC", json_boolean(!!(v & 0x10)));
  json_object_set_new(o, "VIN_UV", json_boolean(!!(v & 0x08)));
  json_object_set_new(o, "TEMPERATURE", json_boolean(!!(v & 0x04)));
  json_object_set_new(o, "CML", json_boolean(!!(v & 0x02)));

  return o;
}

json_t *
decode_status_word(uint16_t w) {
  json_t *o = json_object();

  json_object_set_new(o, "VOUT", json_boolean(!!(w & 0x8000)));
  json_object_set_new(o, "IOUT_POUT", json_boolean(!!(w & 0x4000)));
  json_object_set_new(o, "INPUT", json_boolean(!!(w & 0x2000)));
  json_object_set_new(o, "MFR_SPECIFIC", json_boolean(!!(w & 0x1000)));
  json_object_set_new(o, "POWER_GOOD", json_boolean(!!(w & 0x0800)));
  json_object_set_new(o, "FANS", json_boolean(!!(w & 0x0400)));
  json_object_set_new(o, "OTHER", json_boolean(!!(w & 0x0200)));
  json_object_set_new(o, "UKN", json_boolean(!!(w & 0x0100)));
  json_object_set_new(o, "TEMPERATURE", json_boolean(!!(w & 0x0080)));
  json_object_set_new(o, "CML", json_boolean(!!(w & 0x0040)));
  json_object_set_new(o, "BUSY", json_boolean(!!(w & 0x0020)));
  json_object_set_new(o, "OFF", json_boolean(!!(w & 0x0010)));

  return o;
}

json_t *
decode_status_vout(uint8_t v) {
  json_t *o = bits8(v);

  json_object_set_new(o, "OV_FAULT", json_boolean(!!(v & 0x80)));
  json_object_set_new(o, "OV_WARN", json_boolean(!!(v & 0x40)));
  json_object_set_new(o, "UV_WARN", json_boolean(!!(v & 0x20)));
  json_object_set_new(o, "UV_FAULT", json_boolean(!!(v & 0x10)));

  return o;
}

json_t *
decode_status_iout(uint8_t v) {
  json_t *o = bits8(v);

  json_object_set_new(o, "OC_WARN", json_boolean(!!(v & 0x40)));
  json_object_set_new(o, "OC_FAULT", json_boolean(!!(v & 0x20)));
  json_object_set_new(o, "OC_LV_FAULT", json_boolean(!!(v & 0x10)));
  json_object_set_new(o, "POWER_LIMITED", json_boolean(!!(v & 0x08)));

  return o;
}

json_t *
decode_status_input(uint8_t v) {
  json_t *o = bits8(v);

  json_object_set_new(o, "OV_FAULT", json_boolean(!!(v & 0x80)));
  json_object_set_new(o, "UV_FAULT", json_boolean(!!(v & 0x40)));
  json_object_set_new(o, "OV_WARN", json_boolean(!!(v & 0x20)));
  json_object_set_new(o, "UV_WARN", json_boolean(!!(v & 0x10)));
  json_object_set_new(o, "IOUT_OC_WARN", json_boolean(!!(v & 0x02)));
  json_object_set_new(o, "IIN_OC_WARN", json_boolean(!!(v & 0x01)));

  return o;
}

json_t *
decode_status_temperature(uint8_t v) {
  json_t *o = bits8(v);

  json_object_set_new(o, "OT_FAULT", json_boolean(!!(v & 0x80)));
  json_object_set_new(o, "OT_WARN", json_boolean(!!(v & 0x40)));
  json_object_set_new(o, "UT_WARN", json_boolean(!!(v & 0x20)));
  json_object_set_new(o, "UT_FAULT", json_boolean(!!(v & 0x10)));

  return o;
}

json_t *
decode_status_cml(uint8_t v) {
  json_t *arr = json_array();

  if (v & 0x80)
    json_array_append_new(arr, json_string("invalid_or_unsupported_command"));
  if (v & 0x40)
    json_array_append_new(arr, json_string("invalid_or_unsupported_data"));
  if (v & 0x20)
    json_array_append_new(arr, json_string("pec_failed"));
  if (v & 0x10)
    json_array_append_new(arr, json_string("memory_fault_detected"));
  if (v & 0x02)
    json_array_append_new(arr, json_string("other_comm_fault"));
  if (v & 0x01)
    json_array_append_new(arr, json_string("memory_or_logic_fault"));

  return arr;
}
