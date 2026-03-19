/* * ====================================================================
 * HELTEC V3 LORA & TEMPERATURE MONITOR (RADIOLIB 7.x ROBUST NVS + BATTERY FIXED)
 * ====================================================================
 */

#include <RadioLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <U8g2lib.h>

// --- Hardware Pins Heltec V3 ---
#define VEXT_PIN 36
#define VBAT_READ_CTL 37
#define VBAT_ADC_PIN 1
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define SENSOR_PIN 4
#define PRG_BUTTON 0
#define VBAT_FACTOR 5.31  // Factory default for Heltec V3 divider

// --- RTC Speicher ---
RTC_DATA_ATTR uint32_t bootCount = 0;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, OLED_RST);
OneWire oneWire(SENSOR_PIN);
DallasTemperature sensors(&oneWire);
Preferences prefs;
Preferences loraPrefs;

SX1262 radio = new Module(8, 14, 12, 13);
LoRaWANNode node(&radio, &EU868);

// Konfigurationsdaten
uint64_t devEui = 0, joinEui = 0;
uint8_t appKey[16] = {0};
String deviceName;
DeviceAddress sensorOrder[4];
int savedSensorCount = 0;
int tx_interval_minutes = 10;
int display_duration = 8;
bool display_horizontal = true;

// Alarm thresholds
#define TEMP_MIN -20.0
#define TEMP_MAX  60.0

// --- Hilfsfunktionen ---
void hexToBytes(String hex, uint8_t* bytes, int len) {
  for (int i = 0; i < len; i++) {
    String part = hex.substring(i * 2, i * 2 + 2);
    bytes[i] = (uint8_t) strtol(part.c_str(), NULL, 16);
  }
}

uint64_t hexToUint64(String hex) {
  uint64_t res = 0;
  hex.toUpperCase();
  for (int i = 0; i < hex.length(); i++) {
    char c = hex[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
      res <<= 4;
      if (c >= '0' && c <= '9') res |= (c - '0');
      else if (c >= 'A' && c <= 'F') res |= (c - 'A' + 10);
    }
  }
  return res;
}

String uint64ToHex(uint64_t val) {
  char buf[17];
  sprintf(buf, "%08X%08X", (uint32_t)(val >> 32), (uint32_t)val);
  return String(buf);
}

String bytesToHex(uint8_t* bytes, int len) {
  String res = "";
  for (int i = 0; i < len; i++) {
    if (bytes[i] < 16) res += "0";
    res += String(bytes[i], HEX);
  }
  res.toUpperCase();
  return res;
}

String addrToString(DeviceAddress deviceAddress) {
  String res = "";
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) res += "0";
    res += String(deviceAddress[i], HEX);
  }
  return res;
}

float getBatteryVoltage() {
  pinMode(VBAT_READ_CTL, OUTPUT);
  digitalWrite(VBAT_READ_CTL, HIGH); // HIGH to enable divider on Heltec V3
  delay(100);

  uint32_t raw = 0;
  for (int i = 0; i < 100; i++) { raw += analogRead(VBAT_ADC_PIN); delay(1); }
  float v = (raw / 100.0 / 4095.0) * 3.3 * VBAT_FACTOR;

  // digitalWrite(VBAT_READ_CTL, LOW); // Disable to save power (optional)
  return v;
}

uint8_t batteryPercent(float v) {
    if (v >= 4.2) return 100;
    if (v <= 3.3) return 0;
    return (uint8_t)((v - 3.3) / (4.2 - 3.3) * 100);
}

void loadConfiguration() {
  prefs.begin("loraconfig", true);
  prefs.getBytes("deveui", &devEui, 8);
  prefs.getBytes("appeui", &joinEui, 8);
  prefs.getBytes("appkey", appKey, 16);
  deviceName = prefs.getString("name", "Heltec");
  tx_interval_minutes = prefs.getInt("txInt", 10);
  display_horizontal = prefs.getBool("dispHori", true);
  display_duration = prefs.getInt("dispDur", 8);
  savedSensorCount = prefs.getInt("sCount", 0);
  for (int i = 0; i < 4; i++) {
    prefs.getBytes(("s" + String(i)).c_str(), sensorOrder[i], 8);
  }
  prefs.end();
}

void sendCurrentConfig() {
  Serial.print("LORA_DATA|");
  Serial.print(uint64ToHex(devEui)); Serial.print("|");
  Serial.print(uint64ToHex(joinEui)); Serial.print("|");
  Serial.print(bytesToHex(appKey, 16)); Serial.print("|");
  Serial.print(tx_interval_minutes); Serial.print("|");
  Serial.println(deviceName);
}

void saveLoRaWANToNVS() {
  loraPrefs.begin("loranvs", false);

  uint8_t* sessionBuf = node.getBufferSession();
  loraPrefs.putBytes("session", sessionBuf, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);

  uint8_t* noncesBuf = node.getBufferNonces();
  loraPrefs.putBytes("nonces", noncesBuf, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);

  loraPrefs.putBool("valid", true);
  loraPrefs.end();

  uint32_t fcntUp = node.getFCntUp();
  Serial.printf("NVS SAVE: FCntUp=%u\n", fcntUp);
}

void restoreLoRaWANFromNVS() {
  loraPrefs.begin("loranvs", true);
  if (loraPrefs.getBool("valid", false)) {
    uint8_t sessionBuf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
    uint8_t noncesBuf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];

    loraPrefs.getBytes("nonces", noncesBuf, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    int16_t state = node.setBufferNonces(noncesBuf);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("Nonces aus NVS geladen.");

      loraPrefs.getBytes("session", sessionBuf, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
      state = node.setBufferSession(sessionBuf);
      if (state == RADIOLIB_ERR_NONE) {
        uint32_t fcntUp = node.getFCntUp();
        Serial.printf("Session aus NVS geladen (FCntUp=%u).\n", fcntUp);
      } else {
        Serial.printf("Session restore failed: %d\n", state);
      }
    } else {
      Serial.printf("Nonces restore failed: %d\n", state);
    }
  } else {
    Serial.println("Keine valide LoRaWAN Session im NVS gefunden.");
  }
  loraPrefs.end();
}

void clearLoRaWANFromNVS() {
  loraPrefs.begin("loranvs", false);
  loraPrefs.putBool("valid", false);
  loraPrefs.end();
  Serial.println("NVS Session ungültig gesetzt.");
}

void scanAndSaveSensors() {
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  sensors.begin();
  int found = sensors.getDeviceCount();
  if (found > 4) found = 4;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "SENSOR SCAN:");

  prefs.begin("loraconfig", false);
  prefs.putInt("sCount", found);
  for (int i = 0; i < found; i++) {
    if (sensors.getAddress(sensorOrder[i], i)) {
      prefs.putBytes(("s" + String(i)).c_str(), sensorOrder[i], 8);
      u8g2.setCursor(0, 25 + (i * 10));
      String addr = addrToString(sensorOrder[i]);
      u8g2.printf("S%d: %s", i+1, addr.substring(8).c_str());

      // Python Manager output
      sensors.requestTemperatures();
      float t = sensors.getTempC(sensorOrder[i]);
      Serial.print("ROM:"); Serial.print(addr);
      Serial.print("|TEMP:"); Serial.println(t, 1);
    }
  }
  prefs.end();
  u8g2.sendBuffer();
  delay(3000);
}

void showDisplay(float vbat, const char* overrideMsg = nullptr) {
  digitalWrite(VEXT_PIN, LOW);
  delay(100);
  u8g2.begin();
  u8g2.clearBuffer();
  if (overrideMsg != nullptr) {
    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.drawStr(0, 20, overrideMsg);
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(0, 40, "Befehle via USB.");
    u8g2.sendBuffer();
    return;
  }
  int percent = batteryPercent(vbat);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, deviceName.c_str());
  u8g2.drawHLine(0, 12, 128);

  pinMode(SENSOR_PIN, INPUT_PULLUP);
  sensors.begin();
  sensors.requestTemperatures();

  u8g2.setFont(u8g2_font_7x14_tf);
  for (int i = 0; i < savedSensorCount; i++) {
    float t = sensors.getTempC(sensorOrder[i]);
    String txt = "S" + String(i + 1) + ":" + (t > -50 ? String(t, 1) + "C" : "ERR");
    int x = (display_horizontal) ? (i % 2) * 64 : 5;
    int y = (display_horizontal) ? (i / 2) * 20 + 28 : (i * 15) + 28;
    u8g2.drawStr(x, y, txt.c_str());
  }
  u8g2.setCursor(0, 62);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.printf("%.2fV | %d%% | B:%d", vbat, percent, bootCount);
  u8g2.sendBuffer();
  delay(display_duration * 1000);
  u8g2.setPowerSave(1);
}

void handleSerialConfig() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    if (input == "SCAN") {
      scanAndSaveSensors();
      return;
    }

    if (input.startsWith("SAVE|")) {
      int parts[7];
      int count = 0;
      int startSearch = 0;
      while (count < 7) {
        int idx = input.indexOf('|', startSearch);
        if (idx == -1) break;
        parts[count++] = idx;
        startSearch = idx + 1;
      }

      if (count == 7) {
        String sDevEui = input.substring(parts[0]+1, parts[1]);
        String sAppEui = input.substring(parts[1]+1, parts[2]);
        String sAppKey = input.substring(parts[2]+1, parts[3]);
        String sInterval = input.substring(parts[3]+1, parts[4]);
        String sName = input.substring(parts[4]+1, parts[5]);
        String sHori = input.substring(parts[5]+1, parts[6]);
        String sDur = input.substring(parts[6]+1);

        prefs.begin("loraconfig", false);
        uint64_t tDev = hexToUint64(sDevEui);
        uint64_t tApp = hexToUint64(sAppEui);
        uint8_t tKey[16]; hexToBytes(sAppKey, tKey, 16);

        prefs.putBytes("deveui", &tDev, 8);
        prefs.putBytes("appeui", &tApp, 8);
        prefs.putBytes("appkey", tKey, 16);
        prefs.putInt("txInt", sInterval.toInt());
        prefs.putString("name", sName);
        prefs.putBool("dispHori", sHori == "1");
        prefs.putInt("dispDur", sDur.toInt());
        prefs.end();

        Serial.println("SAVE_OK");
        loadConfiguration();
        clearLoRaWANFromNVS();
      }
      return;
    }
  }
}

void sendLora(float vbat) {
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  sensors.begin();
  sensors.requestTemperatures();

  uint16_t battmV = (uint16_t)(vbat * 1000);
  uint8_t battPct = batteryPercent(vbat);
  uint8_t count = (uint8_t)savedSensorCount;
  uint8_t alarmMask = 0;

  uint8_t payload[40];
  int idx = 0;

  // Format matching decoder.js: mV(2) + %(1) + count(1) + T(2*count) + alarm(1)
  payload[idx++] = (battmV >> 8) & 0xFF;
  payload[idx++] = battmV & 0xFF;
  payload[idx++] = battPct;
  payload[idx++] = count;

  for (int i = 0; i < count; i++) {
    float t = sensors.getTempC(sensorOrder[i]);
    int16_t ti = (t > -50.0) ? (int16_t)(t * 10) : 0x7FFF;
    payload[idx++] = (ti >> 8) & 0xFF;
    payload[idx++] = ti & 0xFF;

    if (t < TEMP_MIN || t > TEMP_MAX) {
      alarmMask |= (1 << i);
    }
  }
  payload[idx++] = alarmMask;

  int state = node.sendReceive(payload, idx);

  if (state >= RADIOLIB_ERR_NONE) {
    saveLoRaWANToNVS();
  } else {
    Serial.printf("Send failed: %d\n", state);
    saveLoRaWANToNVS();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Heltec V3 Boot ---");
  Serial.printf("Boot Count: %d\n", bootCount);
  Serial.printf("Wakeup Cause: %d\n", esp_sleep_get_wakeup_cause());

  pinMode(PRG_BUTTON, INPUT_PULLUP);
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);

  // Batterie-Messschaltung aktivieren (User working example: Pin 37 HIGH)
  pinMode(VBAT_READ_CTL, OUTPUT);
  digitalWrite(VBAT_READ_CTL, HIGH);

  delay(2000);

  loadConfiguration();

  if (digitalRead(PRG_BUTTON) == LOW) {
    bool modeActive = true;
    for (int i = 0; i < 100; i++) {
      delay(100);
      if (digitalRead(PRG_BUTTON) == HIGH) { modeActive = false; break; }
    }
    if (modeActive) {
      sendCurrentConfig();
      showDisplay(getBatteryVoltage(), "MODUS: SETUP");
      while (true) { handleSerialConfig(); delay(10); }
    }
  }

  float v = getBatteryVoltage();
  Serial.printf("Battery: %.2fV\n", v);

  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    node.beginOTAA(joinEui, devEui, NULL, appKey);

    restoreLoRaWANFromNVS();

    if (!node.isActivated()) {
      Serial.println("Starte OTAA Join...");
      state = node.activateOTAA();
      if (state >= RADIOLIB_ERR_NONE) {
        Serial.println("Join erfolgreich!");
        saveLoRaWANToNVS();
      } else {
        Serial.printf("Join fehlgeschlagen: %d\n", state);
        saveLoRaWANToNVS();
      }
    } else {
      Serial.println("Session erfolgreich wiederhergestellt.");
    }
  }

  if (state < RADIOLIB_ERR_NONE && !node.isActivated()) {
    Serial.printf("LoRaWAN init failed: %d\n", state);
  }

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0 || bootCount == 0) {
    showDisplay(v);
  }

  pinMode(SENSOR_PIN, INPUT_PULLUP);
  sensors.begin();
  sensors.requestTemperatures();
  delay(800);

  sendLora(v);
  bootCount++;

  radio.sleep();
  digitalWrite(VEXT_PIN, HIGH);
  esp_sleep_enable_timer_wakeup((uint64_t)tx_interval_minutes * 60 * 1000000);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PRG_BUTTON, 0);
  esp_deep_sleep_start();
}

void loop() {}
