// ─────────────────────────────────────────────
// WICHTIG: LMIC Interrupts AUS (ESP32 Watchdog-Fix)
#define LMIC_USE_INTERRUPTS 0

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <XPowersLib.h>

// ─────────────────────────────────────────────
// AXP2101 Power Management
XPowersAXP2101 pmu;

// ─────────────────────────────────────────────
// T-Beam v1.x LoRa Pinmapping
const lmic_pinmap lmic_pins = {
    .nss  = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst  = 23,
    .dio  = {26, 33, 32},
};

// ─────────────────────────────────────────────
// LoRaWAN OTAA KEYS

// DevEUI: 98fb9a48cf113f1a (little-endian!)
static const u1_t PROGMEM DEVEUI[8] = { 0x1A, 0x3F, 0x11, 0xCF, 0x48, 0x9A, 0xFB, 0x98 };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }

// JoinEUI: 8c2499fd20365455 (little-endian!)
static const u1_t PROGMEM APPEUI[8] = { 0x55, 0x54, 0x36, 0x20, 0xFD, 0x99, 0x24, 0x8C };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }

// AppKey: 302729ebdc97e5d5e4c05c9c36a5f245 (big-endian)
static const u1_t PROGMEM APPKEY[16] = { 0x30, 0x27, 0x29, 0xEB, 0xDC, 0x97, 0xE5, 0xD5, 0xE4, 0xC0, 0x5C, 0x9C, 0x36, 0xA5, 0xF2, 0x45 };
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

// ─────────────────────────────────────────────
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

static osjob_t sendjob;
const unsigned TX_INTERVAL = 120;

// ─────────────────────────────────────────────
// AXP2101 Helpers
void initPMU() {
    Wire.begin(21, 22);
    if (!pmu.init()) {
        Serial.println("Failed to initialize PMU!");
        while (true);
    }
    Serial.println("AXP2101 Power management initialization success");

    // Disable unused power rails
    pmu.disableDC2();
    pmu.disableDC3();
    pmu.disableDC4();
    pmu.disableDC5();
    pmu.disableALDO1();
    pmu.disableALDO4();
    pmu.disableBLDO1();
    pmu.disableBLDO2();
    pmu.disableDLDO1();
    pmu.disableDLDO2();

    // Set voltage for ESP32, LoRa, GPS
    pmu.setDC1Voltage(3300);
    pmu.enableDC1();
    pmu.setALDO2Voltage(3300);
    pmu.enableALDO2();
    pmu.setALDO3Voltage(3300);
    pmu.enableALDO3();

    // Enable Battery ADC
    pmu.enableBattVoltageMeasure();
}

float getBatteryVoltage() {
    if (!pmu.isBatteryConnect()) {
        return 0.0;
    }
    return pmu.getBattVoltage() / 1000.0;
}

// ─────────────────────────────────────────────
// LMIC Events
void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch (ev) {
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;

        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            LMIC_setLinkCheckMode(0);
            break;

        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE"));
            os_setTimedCallback(
                &sendjob,
                os_getTime() + sec2osticks(TX_INTERVAL),
                do_send
            );
            break;

        default:
            break;
    }
}

// ─────────────────────────────────────────────
// Uplink
void do_send(osjob_t* j) {
    if (LMIC.opmode & OP_TXRXPEND) return;

    sensors.requestTemperatures();
    float t1 = sensors.getTempCByIndex(0);
    float t2 = sensors.getTempCByIndex(1);
    float vb = getBatteryVoltage();

    uint8_t payload[6];
    int16_t t1s = (int16_t)(t1 * 100);
    int16_t t2s = (int16_t)(t2 * 100);
    uint16_t vbs = (uint16_t)(vb * 100);

    payload[0] = t1s;
    payload[1] = t1s >> 8;
    payload[2] = t2s;
    payload[3] = t2s >> 8;
    payload[4] = vbs;
    payload[5] = vbs >> 8;

    LMIC_setTxData2(1, payload, sizeof(payload), 0);
    Serial.println(F("Uplink queued"));
}

// ─────────────────────────────────────────────
// SETUP
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println(F("Starting T-Beam OTAA"));

    // SPI FIX (extrem wichtig)
    SPI.begin(5, 19, 27, 18); // SCK, MISO, MOSI, NSS

    initPMU();
    sensors.begin();

    os_init();
    LMIC_reset();

    // Clock-Toleranz (Join stabiler)
    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);

    // ERSTES SENDEN korrekt starten
    os_setCallback(&sendjob, do_send);
}

// ─────────────────────────────────────────────
// LOOP (Watchdog-sicher!)
void loop() {
    os_runloop_once();
    delay(1); // Pflicht für ESP32
}
