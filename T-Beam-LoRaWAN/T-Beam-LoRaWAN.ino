#define LMIC_USE_INTERRUPTS 0

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <XPowersLib.h>

// ─────────────────────────────
#define ONE_WIRE_BUS 25
#define MAX_SENSORS 10
#define TEMP_MIN -20.0
#define TEMP_MAX  60.0

// ─────────────────────────────
XPowersAXP2101 pmu;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ─────────────────────────────
static osjob_t sendjob;
const unsigned TX_INTERVAL = 120;

// ─────────────────────────────
// LMIC PINMAP (T-Beam v1.x)
const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 23,
    .dio = {26, 33, 32}
};

// ─────────────────────────────
// OTAA KEYS
static const u1_t PROGMEM DEVEUI[8] = { 0x1A,0x3F,0x11,0xCF,0x48,0x9A,0xFB,0x98 };
static const u1_t PROGMEM APPEUI[8] = { 0x55,0x54,0x36,0x20,0xFD,0x99,0x24,0x8C };
static const u1_t PROGMEM APPKEY[16]= { 0x30,0x27,0x29,0xEB,0xDC,0x97,0xE5,0xD5,0xE4,0xC0,0x5C,0x9C,0x36,0xA5,0xF2,0x45 };

void os_getDevEui(u1_t* buf){ memcpy_P(buf, DEVEUI, 8); }
void os_getArtEui(u1_t* buf){ memcpy_P(buf, APPEUI, 8); }
void os_getDevKey(u1_t* buf){ memcpy_P(buf, APPKEY, 16); }

// ─────────────────────────────
void initPMU() {
    Wire.begin(21,22);
    pmu.init();
    pmu.enableBattVoltageMeasure();
}

// ─────────────────────────────
uint8_t batteryPercent(float v) {
    if (v >= 4.2) return 100;
    if (v <= 3.3) return 0;
    return (uint8_t)((v - 3.3) / (4.2 - 3.3) * 100);
}

// ─────────────────────────────
void do_send(osjob_t* j) {
    if (LMIC.opmode & OP_TXRXPEND) return;

    sensors.requestTemperatures();
    uint8_t count = sensors.getDeviceCount();
    if (count > MAX_SENSORS) count = MAX_SENSORS;

    float battV = pmu.isBatteryConnect() ? pmu.getBattVoltage()/1000.0 : 0;
    uint16_t battmV = battV * 1000;
    uint8_t battPct = batteryPercent(battV);

    uint8_t payload[32];
    uint8_t idx = 0;
    uint8_t alarmMask = 0;

    payload[idx++] = battmV >> 8;
    payload[idx++] = battmV & 0xFF;
    payload[idx++] = battPct;
    payload[idx++] = count;

    for (uint8_t i = 0; i < count; i++) {
        float t = sensors.getTempCByIndex(i);
        int16_t ts = t * 10;
        payload[idx++] = ts >> 8;
        payload[idx++] = ts & 0xFF;

        if (t < TEMP_MIN || t > TEMP_MAX) {
            alarmMask |= (1 << i);
        }

        Serial.printf("T%d: %.2f°C\n", i, t);
    }

    payload[idx++] = alarmMask;

    Serial.printf("Battery: %.2fV (%d%%)\n", battV, battPct);
    Serial.printf("AlarmMask: 0x%02X\n", alarmMask);

    LMIC_setTxData2(2, payload, idx, 0);
}

// ─────────────────────────────
void onEvent(ev_t ev) {
    if (ev == EV_JOINED) {
        LMIC_setLinkCheckMode(0);
        do_send(&sendjob);
    }
    if (ev == EV_TXCOMPLETE) {
        os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
    }
}

// ─────────────────────────────
void setup() {
    Serial.begin(115200);
    SPI.begin(5,19,27,18);
    initPMU();
    sensors.begin();
    os_init();
    LMIC_reset();
    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
}

// ─────────────────────────────
void loop() {
    os_runloop_once();
    delay(1);
}
