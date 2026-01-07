function decodeUplink(input) {
  var data = {};

  // Temperature 1: bytes 0 (LSB) and 1 (MSB)
  var temp1_raw = (input.bytes[1] << 8) | input.bytes[0];
  // Sign extension for 16-bit signed integer
  if (temp1_raw & 0x8000) {
    temp1_raw = -(0x10000 - temp1_raw);
  }
  data.temperature_1 = temp1_raw / 100.0;

  // Temperature 2: bytes 2 (LSB) and 3 (MSB)
  var temp2_raw = (input.bytes[3] << 8) | input.bytes[2];
  if (temp2_raw & 0x8000) {
    temp2_raw = -(0x10000 - temp2_raw);
  }
  data.temperature_2 = temp2_raw / 100.0;

  // Battery Voltage: bytes 4 (LSB) and 5 (MSB)
  // The LMIC code sends voltage * 100 as a uint16_t
  var batt_raw = (input.bytes[5] << 8) | input.bytes[4];
  data.battery_voltage = batt_raw / 100.0;

  return {
    data: data,
    warnings: [],
    errors: []
  };
}