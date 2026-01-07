// ─────────────────────────────────────────────
// WICHTIG: LMIC Interrupts AUS (ESP32 Watchdog-Fix)
#define LMIC_USE_INTERRUPTS 0

// Force LMIC region & radio
#define CFG_eu868 1
#define CFG_sx1276_radio 1

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ─────────────────────────────────────────────
// AXP192 Power Management
#define AXP192_SLAVE_ADDRESS 0x34

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

// DevEUI: 98fb9a48cf113f1a  (little-endian!)
static const u1_t PROGMEM DEVEUI[8] = {
    0x1A, 0x3F, 0x11, 0xCF, 0x48, 0x9A, 0xFB, 0x98
};
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }

// JoinEUI: 8c2499fd20365455 (little-endian!)
static const u1_t PROGMEM APPEUI[8] = {
    0x55, 0x54, 0x36, 0x20, 0xFD, 0x99, 0x24, 0x8C
};
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }

// AppKey (MSB / big-endian)
// a7c910527794b269425a648d6a247d82
static const u1_t PROGMEM APPKEY[16] = {
    0xA7, 0xC9, 0x10, 0x52,
    0x77, 0x94, 0xB2, 0x69,
    0x42, 0x5A, 0x64, 0x8D,
    0x6A, 0x24, 0x7D, 0x82
};
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

// ─────────────────────────────────────────────
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

static osjob_t sendjob;
const unsigned TX_INTERVAL = 120;

// ─────────────────────────────────────────────
// AXP192 Helpers
void writeAXP192(byte reg, byte value) {
    Wire.beginTransmission(AXP192_SLAVE_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

void initAXP192() {
    Wire.begin(21, 22);
    writeAXP192(0x12, 0x4D); // LDO2 (LoRa) & LDO3 (GPS) ON
    writeAXP192(0x28, 0x0C); // LDO2 = 3.3V
    writeAXP192(0x82, 0xFF); // ADCs ON
}

float getBatteryVoltage() {
    Wire.beginTransmission(AXP192_SLAVE_ADDRESS);
    Wire.write(0x78);
    Wire.endTransmission();
    Wire.requestFrom(AXP192_SLAVE_ADDRESS, 2);
    uint16_t v = (Wire.read() << 4) | (Wire.read() & 0x0F);
    return v * 1.1 / 1000.0;
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

    initAXP192();
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
