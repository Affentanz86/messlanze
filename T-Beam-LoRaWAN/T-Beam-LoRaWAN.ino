#include <RadioLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <axp20x.h>

// LoRaWAN Credentials
uint64_t joinEUI = 0x8c2499fd20365455;
uint64_t devEUI = 0x98fb9a48cf113f1a;
uint8_t appKey[] = {0x30, 0x27, 0x29, 0xeb, 0xdc, 0x97, 0xe5, 0xd5, 0xe4, 0xc0, 0x5c, 0x9c, 0x36, 0xa5, 0xf2, 0x45};
uint8_t nwkKey[] = {0x30, 0x27, 0x29, 0xeb, 0xdc, 0x97, 0xe5, 0xd5, 0xe4, 0xc0, 0x5c, 0x9c, 0x36, 0xa5, 0xf2, 0x45};

// DS18B20 Sensor Pin
#define ONE_WIRE_BUS 13

// Other Constants
#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  300        // Time ESP32 will go to sleep (in seconds)

// Module instances
// IMPORTANT: Make sure the pin numbers match your specific board version.
SX1276 radio = new Module(18, 26, 14, 33);
LoRaWANNode node(&radio, &EU868);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AXP20X_Class pmu;

void setup() {
  Serial.begin(115200);

  // Initialize PMU
  Wire.begin(21, 22);
  if (!pmu.begin(Wire, AXP192_SLAVE_ADDRESS)) {
    Serial.println("Failed to initialize PMU!");
    while (true);
  }

  // Initialize sensors
  sensors.begin();

  // Initialize LoRa radio
  Serial.print(F("[SX1276] Initializing ... "));
  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }
  Serial.println(F("success!"));

  // Join LoRaWAN network
  Serial.print(F("[LoRaWAN] Joining network ... "));
  state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }
  Serial.println(F("success!"));
}

void loop() {
  // Request temperatures
  sensors.requestTemperatures();
  float temp1 = sensors.getTempCByIndex(0);
  float temp2 = sensors.getTempCByIndex(1);
  float batteryVoltage = pmu.getBattVoltage() / 1000.0f;

  // Print sensor readings
  Serial.print("Temperature 1: ");
  Serial.print(temp1);
  Serial.println(" *C");
  Serial.print("Temperature 2: ");
  Serial.print(temp2);
  Serial.println(" *C");
  Serial.print("Battery Voltage: ");
  Serial.print(batteryVoltage);
  Serial.println(" V");

  // Prepare payload
  byte payload[6];
  int16_t temp1_int = temp1 * 100;
  int16_t temp2_int = temp2 * 100;
  uint16_t voltage_int = batteryVoltage * 100;

  payload[0] = (temp1_int >> 8) & 0xFF;
  payload[1] = temp1_int & 0xFF;
  payload[2] = (temp2_int >> 8) & 0xFF;
  payload[3] = temp2_int & 0xFF;
  payload[4] = (voltage_int >> 8) & 0xFF;
  payload[5] = voltage_int & 0xFF;

  // Send LoRaWAN packet
  Serial.print(F("[LoRaWAN] Sending packet ... "));
  int state = node.sendReceive(payload, sizeof(payload));
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }

  // Go to deep sleep
  Serial.print("Entering deep sleep for ");
  Serial.print(TIME_TO_SLEEP);
  Serial.println(" seconds...");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}
