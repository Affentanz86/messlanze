function decodeUplink(input) {
  var data = {};
  data.temp1 = ((input.bytes[1] << 8) | input.bytes[0]) / 100.0;
  data.temp2 = ((input.bytes[3] << 8) | input.bytes[2]) / 100.0;
  data.voltage = ((input.bytes[5] << 8) | input.bytes[4]) / 100.0;
  return {
    data: data,
    warnings: [],
    errors: []
  };
}
