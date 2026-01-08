// Chirpstack v4 JavaScript Codec
// ----------------------------------
// Docs: https://www.chirpstack.io/docs/chirpstack/use/device-profiles/codec.html
//
// Payload-Struktur (6 Bytes, Little-Endian):
// [0-1]: Temperatur 1 (signed int16, Wert * 100)
// [2-3]: Temperatur 2 (signed int16, Wert * 100)
// [4-5]: Batteriespannung (unsigned int16, Wert * 100)
// ----------------------------------

function decodeUplink(input) {
  // Überprüfen, ob die erwartete Anzahl von Bytes empfangen wurde.
  if (input.bytes.length !== 6) {
    return {
      errors: ["Erwartet wurden 6 Bytes, empfangen wurden " + input.bytes.length]
    };
  }

  // Erstellen eines ArrayBuffer und DataView aus dem Byte-Array
  // DataView ermöglicht das Lesen von Multi-Byte-Zahlen aus dem Puffer.
  var buffer = new ArrayBuffer(input.bytes.length);
  var view = new DataView(buffer);
  input.bytes.forEach(function (b, i) {
    view.setUint8(i, b);
  });

  var decoded = {};

  // Temperatur 1: Bytes 0-1, vorzeichenbehaftete 16-Bit-Ganzzahl, Little-Endian
  // Der 'true'-Parameter gibt Little-Endian an.
  decoded.temperature_1 = view.getInt16(0, true) / 100.0;

  // Temperatur 2: Bytes 2-3, vorzeichenbehaftete 16-Bit-Ganzzahl, Little-Endian
  decoded.temperature_2 = view.getInt16(2, true) / 100.0;

  // Batteriespannung: Bytes 4-5, vorzeichenlose 16-Bit-Ganzzahl, Little-Endian
  decoded.battery_voltage = view.getUint16(4, true) / 100.0;

  return {
    data: decoded
  };
}
