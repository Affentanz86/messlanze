function decodeUplink(input) {
  var data = {};

  // Temperature 1: bytes 0 and 1
  // The value is signed, so we need to handle negative temperatures
  var temp1_raw = (input.bytes[0] << 8) | input.bytes[1];
  // Sign extension for 16-bit signed integer
  if (temp1_raw & 0x8000) {
    temp1_raw = -(0x10000 - temp1_raw);
  }
  data.temperature_1 = temp1_raw / 100.0;

  // Temperature 2: bytes 2 and 3
  var temp2_raw = (input.bytes[2] << 8) | input.bytes[3];
  if (temp2_raw & 0x8000) {
    temp2_raw = -(0x10000 - temp2_raw);
  }
  data.temperature_2 = temp2_raw / 100.0;

  // Battery Voltage: bytes 4 and 5
  // The value is unsigned
  var batt_mv = (input.bytes[4] << 8) | input.bytes[5];
  data.battery_voltage = batt_mv / 1000.0;

  return {
    data: data,
    warnings: [],
    errors: []
  };
}