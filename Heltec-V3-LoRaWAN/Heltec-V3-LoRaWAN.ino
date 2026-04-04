#include <RadioLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include <Wire.h>

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

// Display Initialisierung (SW_I2C für maximale Kompatibilität)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, OLED_RST);

OneWire oneWire(SENSOR_PIN);
DallasTemperature sensors(&oneWire);
Preferences prefs;

SX1262 radio = new Module(8, 14, 12, 13);
LoRaWANNode node(&radio, &EU868);

// Globale Variablen
uint64_t devEui = 0, joinEui = 0;
uint8_t appKey[16];
String deviceName = "Heltec";
DeviceAddress sensorOrder[4];
int savedSensorCount = 0;
int tx_interval_minutes = 10;
int display_duration = 8;
bool display_horizontal = true;
unsigned long lastSend = 0;
bool loraActive = false;
unsigned long lastJoinAttempt = 0;

void hexToBytes(String hex, uint8_t* bytes, int len) {
  for (int i = 0; i < len; i++) {
    bytes[i] = strtol(hex.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
  }
}

float getBatteryVoltage() {
  long sum = 0;
  for (int i = 0; i < 100; i++) {
    sum += analogRead(VBAT_ADC_PIN);
    delay(1);
  }
  return ((float)sum / 100 / 4095.0) * 3.3 * VBAT_FACTOR;
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

  // WICHTIG: Sende Daten an Python Manager
  Serial.print("LORA_DATA|");
  Serial.print(String((uint32_t)(devEui >> 32), HEX) + String((uint32_t)devEui, HEX)); Serial.print("|");
  Serial.print(String((uint32_t)(joinEui >> 32), HEX) + String((uint32_t)joinEui, HEX)); Serial.print("|");
  for(int i=0; i<16; i++) { if(appKey[i]<16) Serial.print("0"); Serial.print(appKey[i], HEX); }
  Serial.printf("|%d|%s|%d|%d\n", tx_interval_minutes, deviceName.c_str(), display_horizontal ? 1 : 0, display_duration);
}

void showDisplay() {
  float vbat = getBatteryVoltage();
  int percent = (int)((vbat - 3.3) / (4.10 - 3.3) * 100.0);
  percent = constrain(percent, 0, 100);
  bool isCharging = (vbat >= 4.03);

  u8g2.setPowerSave(0);
  u8g2.clearBuffer();

  // Batterie-Icon
  u8g2.drawFrame(100, 2, 24, 10);
  u8g2.drawBox(124, 4, 2, 6);
  int barWidth = (percent * 20) / 100;
  if (barWidth > 0) u8g2.drawBox(102, 4, barWidth, 6);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, isCharging ? "USB LADEN" : deviceName.c_str());
  u8g2.drawHLine(0, 12, 128);

  u8g2.setFont(u8g2_font_7x14_tf);
  sensors.requestTemperatures();
  for (int i = 0; i < savedSensorCount; i++) {
    float t = sensors.getTempC(sensorOrder[i]);
    String txt = "S" + String(i + 1) + ":" + (t > -50 ? String(t, 1) + "C" : "ERR");
    if (display_horizontal) {
      u8g2.drawStr((i % 2) * 64, (i / 2) * 20 + 28, txt.c_str());
    } else {
      u8g2.drawStr(5, (i * 15) + 28, txt.c_str());
    }
  }

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 62);
  u8g2.print(vbat, 2); u8g2.print("V | "); u8g2.print(percent); u8g2.print("%");

  u8g2.sendBuffer();
  delay(display_duration * 1000);
  u8g2.setPowerSave(1);
}

void handleSerial() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "SCAN") {
      sensors.begin();
      int count = sensors.getDeviceCount();
      for (int i = 0; i < count; i++) {
        DeviceAddress addr;
        if (sensors.getAddress(addr, i)) {
          Serial.print("ROM:");
          for (uint8_t b : addr) { if (b < 16) Serial.print("0"); Serial.print(b, HEX); }
          Serial.printf("|TEMP:%.2f\n", sensors.getTempCByIndex(i));
        }
      }
      Serial.println("SCAN_DONE");
    }
    else if (cmd.startsWith("SAVE|")) {
      prefs.begin("loraconfig", false);
      String vals[9];
      int currentPart = 0; int start = 0;
      for (int i = 0; i < (int)cmd.length() && currentPart < 9; i++) {
        if (cmd[i] == '|' || i == (int)cmd.length() - 1) {
          int end = (i == (int)cmd.length() - 1) ? i + 1 : i;
          vals[currentPart++] = cmd.substring(start, end);
          start = i + 1;
        }
      }
      if (currentPart >= 8) {
        uint64_t de = strtoull(vals[1].c_str(), NULL, 16);
        uint64_t ae = strtoull(vals[2].c_str(), NULL, 16);
        uint8_t ak[16]; hexToBytes(vals[3], ak, 16);
        prefs.putBytes("deveui", &de, 8);
        prefs.putBytes("appeui", &ae, 8);
        prefs.putBytes("appkey", ak, 16);
        prefs.putInt("txInt", vals[4].toInt());
        prefs.putString("name", vals[5]);
        prefs.putBool("dispHori", vals[6] == "1");
        prefs.putInt("dispDur", vals[7].toInt());
        if (currentPart == 9 && vals[8].length() > 0) {
          int sCount = 0; String roms = vals[8]; int rStart = 0;
          for (int j = 0; j <= (int)roms.length(); j++) {
            if (roms[j] == ',' || j == (int)roms.length()) {
              String oneRom = roms.substring(rStart, j);
              if (oneRom.length() == 16) {
                DeviceAddress da; hexToBytes(oneRom, da, 8);
                prefs.putBytes(("s" + String(sCount)).c_str(), da, 8);
                sCount++;
              }
              rStart = j + 1;
              if (sCount >= 4) break;
            }
          }
          prefs.putInt("sCount", sCount);
        }
        prefs.end();
        Serial.println("OK:SAVED");
        delay(500); ESP.restart();
      }
    }
  }
}

void sendLora() {
  sensors.requestTemperatures();
  uint8_t payload[40];
  float vbat = getBatteryVoltage();
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
  node.sendReceive(payload, 10 + nLen);
}

void tryJoin() {
  int state = node.activateOTAA();
  if (state >= RADIOLIB_ERR_NONE) {
    loraActive = true;
    lastSend = millis();
  } else {
    loraActive = false;
    lastJoinAttempt = millis();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("System Start...");

  // Power Management
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);  // Vext ein (OLED & Sensoren)

  // Heltec V3: VBAT_READ_CTL muss LOW sein, um die Batterie zu messen
  pinMode(VBAT_READ_CTL, OUTPUT);
  digitalWrite(VBAT_READ_CTL, LOW);

  Serial.println("Warte auf Spannungsstabilisierung (2s)...");
  delay(2000);

  // OneWire Bus Reset
  Serial.println("OneWire Bus Reset...");
  pinMode(SENSOR_PIN, OUTPUT);
  digitalWrite(SENSOR_PIN, LOW);
  delay(100);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  delay(100);

  // Sensor Initialisierung
  int retry = 0;
  while(retry < 3) {
    sensors.begin();
    int count = sensors.getDeviceCount();
    Serial.printf("Versuch %d: %d Sensoren gefunden.\n", retry + 1, count);
    if(count > 0) {
      sensors.requestTemperatures();
      break;
    }
    delay(1000);
    retry++;
  }

  u8g2.begin();
  loadConfiguration();
  if (radio.begin() == RADIOLIB_ERR_NONE) {
    node.beginOTAA(joinEui, devEui, NULL, appKey);
    tryJoin();
  }
}

void loop() {
  handleSerial();
  if (digitalRead(PRG_BUTTON) == LOW) {
    showDisplay();
    if(loraActive) sendLora();
  }
  unsigned long currentMillis = millis();

  // Alle 5 Sek. Batterie-Info an Python senden
  static unsigned long lastBatLog = 0;
  if(currentMillis - lastBatLog > 5000) {
      float v = getBatteryVoltage();
      Serial.printf("BAT:%.2fV|%s\n", v, (v >= 4.03) ? "LADEN" : "AKKU");
      lastBatLog = currentMillis;
  }

  if (!loraActive && (currentMillis - lastJoinAttempt > 30000)) tryJoin();
  if (loraActive && (currentMillis - lastSend >= (unsigned long)tx_interval_minutes * 60000)) {
    lastSend = currentMillis;
    sendLora();
  }
}