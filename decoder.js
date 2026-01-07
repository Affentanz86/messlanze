function decodeUplink(input) {
  var data = {};
  data.temp1 = ((input.bytes[0] << 8) | input.bytes[1]) / 100.0;
  data.temp2 = ((input.bytes[2] << 8) | input.bytes[3]) / 100.0;
  data.battery_voltage = ((input.bytes[4] << 8) | input.bytes[5]) / 1000.0;
  return {
    data: data,
    warnings: [],
    errors: []
  };
}