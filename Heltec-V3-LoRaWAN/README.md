# Heltec V3 LoRaWAN DS18B20 Node

This project is an Arduino sketch for the Heltec WiFi LoRa 32 V3 board that reads temperature data from DS18B20 sensors and transmits it via LoRaWAN.

## Hardware Troubleshooting: DS18B20 on Battery Power

If your sensors work when connected to USB but stop working when running on battery, check the following:

### 1. Power Source for Sensors
* **The Problem:** Many users connect the VCC of their sensors to the **5V or Vext pin**. On many Heltec boards, the 5V pin is ONLY powered when USB is connected. When running on battery, this pin might be dead.
* **The Solution:** Connect the VCC of your DS18B20 sensors to the **3.3V pin** of the Heltec V3. The 3.3V rail remains active on battery power.

### 2. Pull-up Resistor
* OneWire sensors require a pull-up resistor (typically 4.7kΩ) between the DATA pin and VCC (3.3V).
* If the external pull-up is missing or too weak, the internal pull-up of the ESP32 might not be enough.
* **Update in Code:** We have enabled `INPUT_PULLUP` on the sensor pin in the code to provide additional stability, but a physical resistor is still highly recommended.

### 3. Vext Control
* The Heltec V3 uses a MOSFET to control power to certain peripherals (like the OLED display and sometimes external sensors if connected to Vext).
* The code ensures `VEXT_PIN` (GPIO 36) is set to `LOW` to enable power.
* **Update in Code:** We increased the stabilization delay after enabling Vext to 1000ms to ensure the voltage is stable before the sensors are initialized.

### 4. Sensor Initialization Retry
* Sometimes sensors take a moment to "wake up" when power is first applied.
* **Update in Code:** The sketch now includes a retry loop that attempts to initialize the sensors up to 3 times if none are detected initially.

## Compilation
To compile this project using `arduino-cli`:
```bash
arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 Heltec-V3-LoRaWAN
```
