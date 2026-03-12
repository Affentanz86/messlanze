# Heltec V3 LoRaWAN DS18B20 Node

Dieses Projekt ist ein Arduino-Sketch für das Heltec WiFi LoRa 32 V3 Board, das Temperaturdaten von DS18B20-Sensoren liest und über LoRaWAN überträgt.

## Hardware-Fehlersuche: DS18B20 im Batteriebetrieb

Falls Ihre Sensoren funktionieren, wenn das Board über USB angeschlossen ist, aber im Batteriebetrieb ausfallen, prüfen Sie bitte folgende Punkte:

### 1. Stromquelle für Sensoren
* **Das Problem:** Viele Benutzer verbinden den VCC-Pin ihrer Sensoren mit dem **5V- oder Vext-Pin**. Bei vielen Heltec-Boards wird der 5V-Pin NUR mit Strom versorgt, wenn USB angeschlossen ist. Im Batteriebetrieb ist dieser Pin oft spannungslos.
* **Die Lösung:** Verbinden Sie den VCC-Pin Ihrer DS18B20-Sensoren mit dem **3.3V-Pin** des Heltec V3. Die 3.3V-Schiene bleibt auch im Batteriebetrieb aktiv.

### 2. Pull-up-Widerstand
* OneWire-Sensoren benötigen einen Pull-up-Widerstand (typischerweise 4,7 kΩ) zwischen dem DATA-Pin und VCC (3,3 V).
* Falls der externe Widerstand fehlt oder zu schwach ist, reicht der interne Pull-up des ESP32 eventuell nicht aus.
* **Update im Code:** Wir haben `INPUT_PULLUP` für den Sensor-Pin im Code aktiviert, um zusätzliche Stabilität zu bieten. Ein physischer Widerstand wird dennoch dringend empfohlen.

### 3. Vext-Steuerung
* Das Heltec V3 verwendet einen MOSFET, um die Stromversorgung bestimmter Peripheriegeräte (wie das OLED-Display und manchmal externe Sensoren, falls an Vext angeschlossen) zu steuern.
* Der Code stellt sicher, dass `VEXT_PIN` (GPIO 36) auf `LOW` gesetzt wird, um die Stromversorgung zu aktivieren.
* **Update im Code:** Wir haben die Stabilisierungszeit nach dem Aktivieren von Vext auf 1000 ms erhöht, um sicherzustellen, dass die Spannung stabil ist, bevor die Sensoren initialisiert werden.

### 4. Wiederholungsversuch bei der Sensor-Initialisierung
* Manchmal benötigen Sensoren einen Moment, um "aufzuwachen", wenn die Spannung zum ersten Mal angelegt wird.
* **Update im Code:** Der Sketch enthält nun eine Wiederholungsschleife, die bis zu 3 Versuche unternimmt, die Sensoren zu initialisieren, falls anfangs keine erkannt werden.

## Kompilierung
Um dieses Projekt mit `arduino-cli` zu kompilieren:
```bash
arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 Heltec-V3-LoRaWAN
```
