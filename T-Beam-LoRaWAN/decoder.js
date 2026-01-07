function decodeUplink(input) {
  var data = {};

  // Temperature 1
  var temp1_raw = (input.bytes[0] << 8) | input.bytes[1];
  if (temp1_raw > 32767) {
    temp1_raw = temp1_raw - 65536;
  }
  data.temp1 = temp1_raw / 100.0;

  // Temperature 2
  var temp2_raw = (input.bytes[2] << 8) | input.bytes[3];
  if (temp2_raw > 32767) {
    temp2_raw = temp2_raw - 65536;
  }
  data.temp2 = temp2_raw / 100.0;

  // Voltage
  data.voltage = ((input.bytes[4] << 8) | input.bytes[5]) / 100.0;

  return {
    data: data,
    warnings: [],
    errors: []
  };
}
