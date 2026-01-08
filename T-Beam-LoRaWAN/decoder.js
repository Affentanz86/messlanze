// Chirpstack v4 Codec
function decodeUplink(input) {
  var data = {};
  var warnings = [];
  var errors = [];
  var bytes = input.bytes;
  var fPort = input.fPort;

  // Mindestlänge prüfen: 2 (batt) + 1 (pct) + 1 (count) + 1 (alarm) = 5 Bytes
  // Dies ist der Fall, wenn 0 Sensoren angeschlossen sind.
  if (bytes.length < 5) {
    errors.push("Payload length is too short.");
    return {
      data: data,
      warnings: warnings,
      errors: errors
    };
  }

  // DataView verwenden für einfacheres Handling von Multi-Byte-Werten (Big-Endian ist Standard)
  var view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  var idx = 0;

  // Batteriespannung in mV (uint16)
  var battmV = view.getUint16(idx);
  data.battery_mv = battmV;
  data.battery_v = battmV / 1000.0;
  idx += 2;

  // Batterie in Prozent (uint8)
  data.battery_pct = view.getUint8(idx);
  idx += 1;

  // Anzahl der Sensoren (uint8)
  var sensorCount = view.getUint8(idx);
  data.sensor_count = sensorCount;
  idx += 1;

  // Prüfen, ob die Payload-Länge mit der erwarteten Länge basierend auf der Sensoranzahl übereinstimmt
  var expectedLength = 5 + (sensorCount * 2);
  if (bytes.length !== expectedLength) {
      errors.push("Payload length " + bytes.length + " does not match expected length " + expectedLength + " for " + sensorCount + " sensors.");
      // Trotzdem versuchen zu dekodieren, was möglich ist, aber den Fehler zurückgeben
  }

  // Temperaturen auslesen (int16 * 10 für jeden Sensor)
  for (var i = 0; i < sensorCount; i++) {
    // Sicherstellen, dass wir nicht über das Ende des Puffers hinaus lesen
    if (idx + 2 > bytes.length) {
        errors.push("Not enough bytes for sensor " + i);
        break;
    }
    var temp_scaled = view.getInt16(idx);
    // Einen dynamischen Schlüssel für jeden Temperatursensor verwenden
    data['temperature_' + i] = temp_scaled / 10.0;
    idx += 2;
  }

  // Alarm-Maske auslesen (uint8)
  // Sicherstellen, dass wir nicht über das Ende des Puffers hinaus lesen
  if (idx < bytes.length) {
    data.alarm_mask = view.getUint8(idx);
    idx += 1;
  }

  return {
    data: data,
    warnings: warnings,
    errors: errors
  };
}

// Chirpstack v3 Codec (zur Kompatibilität)
function Decode(fPort, bytes) {
    var input = {
        "fPort": fPort,
        "bytes": bytes
    };
    return decodeUplink(input).data;
}
