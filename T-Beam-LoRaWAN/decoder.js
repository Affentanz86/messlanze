// Chirpstack v4 JavaScript Codec für Dragino D22-LB Payload-Format
// --------------------------------------------------------------------
// Angepasst für das T-Beam Projekt.
// Feldnamen wurden an die Standard-Darstellung in Chirpstack für Dragino-Sensoren angeglichen.
//
// Payload-Struktur (11 Bytes, Big-Endian, FPort=2):
// [0-1]: Batteriespannung (unsigned int16, in Millivolt)
// [2-3]: Temperatur Sonde 1 (DS18B20_1, signed int16, Wert * 10)
// [4-5]: Ignoriert
// [6]:   Alarm-Flag
// [7-8]: Temperatur Sonde 2 (DS18B20_2, signed int16, Wert * 10)
// [9-10]: Platzhalter für Sonde 3 (0x7FFF)
// --------------------------------------------------------------------

function decodeUplink(input) {
  // Überprüfen, ob die erwartete Anzahl von Bytes empfangen wurde.
  if (input.bytes.length !== 11) {
    return {
      errors: ["Erwartet wurden 11 Bytes, empfangen wurden " + input.bytes.length]
    };
  }

  // Überprüfen, ob der FPort korrekt ist.
  if (input.fPort !== 2) {
      return {
          warnings: ["Uplink auf falschem FPort empfangen, erwartet 2, war " + input.fPort]
      };
  }

  // Erstellen eines ArrayBuffer und DataView aus dem Byte-Array.
  // DataView ist der robusteste Weg, Multi-Byte-Werte zu lesen.
  // Der Standard ist Big-Endian, was dem Dragino-Format entspricht.
  var buffer = new ArrayBuffer(input.bytes.length);
  var view = new DataView(buffer);
  input.bytes.forEach(function (b, i) {
    view.setUint8(i, b);
  });

  var decoded = {};

  // Bytes 0-1: Batteriespannung (BatV, unsigned int16)
  // Der Wert wird in mV gesendet, wir konvertieren ihn in V.
  decoded.BatV = view.getUint16(0) / 1000.0;

  // Bytes 2-3: Temperatur Sonde 1 (DS18B20_1, signed int16)
  // Der Wert wird als Grad * 10 gesendet, wir teilen, um den echten Wert zu erhalten.
  decoded.DS18B20_1 = view.getInt16(2) / 10.0;

  // Bytes 7-8: Temperatur Sonde 2 (DS18B20_2, signed int16)
  decoded.DS18B20_2 = view.getInt16(7) / 10.0;

  // Byte 6: Alarm-Flag (Ext_Trigger)
  // Wir extrahieren das unterste Bit, um den Alarmstatus zu bestimmen.
  var alarm_byte = view.getUint8(6);
  decoded.Ext_Trigger = (alarm_byte & 0x01) ? true : false;

  return {
    data: decoded
  };
}
