/* * ====================================================================
 * HELTEC V3 LORA & TEMPERATURE MONITOR (RADIOLIB 7.x FIXED + PYTHON COMPATIBLE)
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
#define VBAT_FACTOR 5.31

// --- RTC Speicher ---
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint8_t sessionBuffer[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
RTC_DATA_ATTR bool sessionValid = false;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, OLED_RST);
OneWire oneWire(SENSOR_PIN);
DallasTemperature sensors(&oneWire);
Preferences prefs;

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
  digitalWrite(VBAT_READ_CTL, LOW); // Heltec V3: LOW to enable divider
  delay(50);
  long sum = 0;
  for (int i = 0; i < 100; i++) { sum += analogRead(VBAT_ADC_PIN); delay(1); }
  float vbat = ((float)sum / 100 / 4095.0) * 3.3 * VBAT_FACTOR;
  digitalWrite(VBAT_READ_CTL, HIGH); // Disable to save power
  return vbat;
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
  int percent = constrain((int)((vbat - 3.3) / (4.10 - 3.3) * 100.0), 0, 100);
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
      // Format: SAVE|deveui|appeui|appkey|interval|name|horizontal|duration
      // The Python script sends: cmd = f"SAVE|{eui1}|{eui2}|{key}|{intv}|{name}|{hori}|{dur}\n"
      // There are 7 fields after "SAVE|".

      int parts[7]; // We need to find 7 delimiters
      int count = 0;
      int startSearch = 0;
      while (count < 7) {
        int idx = input.indexOf('|', startSearch);
        if (idx == -1) break;
        parts[count++] = idx;
        startSearch = idx + 1;
      }

      if (count == 7) {
        // parts[0] is index of '|' after SAVE
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
        sessionValid = false;
      }
      return;
    }

    // Fallback for legacy commands
    prefs.begin("loraconfig", false);
    if (input.startsWith("DEVEUI:")) {
      uint64_t tempEui = hexToUint64(input.substring(7));
      prefs.putBytes("deveui", &tempEui, 8); Serial.println("OK: DevEUI");
      sessionValid = false;
    }
    else if (input.startsWith("APPEUI:")) {
      uint64_t tempEui = hexToUint64(input.substring(7));
      prefs.putBytes("appeui", &tempEui, 8); Serial.println("OK: AppEUI");
    }
    else if (input.startsWith("APPKEY:")) {
      uint8_t tempKey[16]; hexToBytes(input.substring(7), tempKey, 16);
      prefs.putBytes("appkey", tempKey, 16); Serial.println("OK: AppKey");
      sessionValid = false;
    }
    else if (input.startsWith("NAME:")) {
      prefs.putString("name", input.substring(5)); Serial.println("OK: Name");
    }
    else if (input.startsWith("TXINT:")) {
      prefs.putInt("txInt", input.substring(6).toInt()); Serial.println("OK: Intervall");
    }
    prefs.end();
  }
}

void sendLora(float vbat) {
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  sensors.begin();
  sensors.requestTemperatures();
  uint8_t payload[40];
  uint16_t v = (uint16_t)(vbat * 100);
  payload[0] = v >> 8; payload[1] = v & 0xFF;
  for (int i = 0; i < 4; i++) {
    float t = (i < savedSensorCount) ? sensors.getTempC(sensorOrder[i]) : -127.0;
    int16_t ti = (t > -50.0) ? (int16_t)(t * 10) : 0x7FFF;
    payload[2+(i*2)] = ti >> 8; payload[3+(i*2)] = ti & 0xFF;
  }
  int nLen = deviceName.length();
  if (nLen > 15) nLen = 15;
  for (int i = 0; i < nLen; i++) payload[10+i] = (uint8_t)deviceName[i];

  int state = node.sendReceive(payload, 10 + nLen);

  if (state >= RADIOLIB_ERR_NONE) {
    uint8_t* nodeBuffer = node.getBufferSession();
    memcpy(sessionBuffer, nodeBuffer, RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
    sessionValid = true;
    Serial.println("Session gesichert.");
  } else {
    Serial.printf("Send failed: %d\n", state);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PRG_BUTTON, INPUT_PULLUP);
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(2000); // Vext stabilization for DS18B20 on battery

  loadConfiguration();

  if (digitalRead(PRG_BUTTON) == LOW) {
    bool modeActive = true;
    for (int i = 0; i < 100; i++) {
      delay(100);
      if (digitalRead(PRG_BUTTON) == HIGH) { modeActive = false; break; }
    }
    if (modeActive) {
      sendCurrentConfig();
      showDisplay(0, "MODUS: SETUP");
      while (true) { handleSerialConfig(); delay(10); }
    }
  }

  float v = getBatteryVoltage();

  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    // Always call beginOTAA first
    node.beginOTAA(joinEui, devEui, NULL, appKey);
    if (sessionValid) {
      state = node.setBufferSession(sessionBuffer);
      if (state >= RADIOLIB_ERR_NONE) {
        Serial.println("Session wiederhergestellt!");
      } else {
        Serial.printf("Session restore failed: %d, joining...\n", state);
        state = node.activateOTAA();
      }
    } else {
      state = node.activateOTAA();
    }
  }

  if (state < RADIOLIB_ERR_NONE) {
    Serial.printf("LoRaWAN init failed: %d\n", state);
  }

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0 || bootCount == 0) {
    showDisplay(v);
  }

  // Dummy request to verify sensor bus
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
