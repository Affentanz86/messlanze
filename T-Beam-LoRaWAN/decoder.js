// Chirpstack v4 Codec for Heltec V3 & T-Beam Fixed Payload
function decodeUplink(input) {
  var data = {};
  var warnings = [];
  var bytes = input.bytes;

  // Fixed length payload: 13 Bytes
  // Byte 0-1: Battery Voltage (mV)
  // Byte 2-9: 4x Temperatures (int16 * 10)
  // Byte 10: Battery Percentage (%)
  // Byte 11: Sensor Count
  // Byte 12: Alarm Mask
  if (bytes.length < 13) {
    return {
      data: data,
      errors: ["Payload too short: expected 13 bytes, got " + bytes.length]
    };
  }

  var view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);

  // 1. Battery Voltage
  var battmV = view.getUint16(0);
  data.battery_v = battmV / 1000.0;
  data.battery_mv = battmV;

  // 2. Temperatures (Fixed slots 1-4)
  for (var i = 0; i < 4; i++) {
    var rawTemp = view.getInt16(2 + (i * 2));
    var key = "temp_s" + (i + 1);

    // 0x7FFF is used as "no data" marker
    if (rawTemp === 0x7FFF) {
      data[key] = null;
    } else {
      data[key] = rawTemp / 10.0;
    }
  }

  // 3. Metadata
  data.battery_pct = view.getUint8(10);
  data.sensor_count = view.getUint8(11);
  data.alarm_mask = view.getUint8(12);

  return {
    data: data,
    warnings: warnings
  };
}

// Chirpstack v3 Codec (Legacy)
function Decode(fPort, bytes) {
    var input = {
        "fPort": fPort,
        "bytes": bytes
    };
    var res = decodeUplink(input);
    return res.data;
}
