# Heltec V3 LoRaWAN DS18B20 Node

Dieses Projekt ist ein Arduino-Sketch für das Heltec WiFi LoRa 32 V3 Board, das Temperaturdaten von DS18B20-Sensoren liest und über LoRaWAN überträgt.

## Hardware-Fehlersuche: DS18B20 im Batteriebetrieb

Falls Ihre Sensoren funktionieren, wenn das Board über USB angeschlossen ist, aber im Batteriebetrieb ausfallen (ERR im Display), prüfen Sie bitte folgende Punkte:

### 1. Stromquelle für Sensoren (WICHTIGST)
* **Das Problem:** Der **5V-Pin** am Heltec Board ist **spannungslos**, wenn kein USB-Kabel angeschlossen ist.
* **Die Lösung:** Verbinden Sie den VCC-Pin Ihrer Sensoren zwingend mit dem **3.3V-Pin**. Nur dieser Pin führt im Batteriebetrieb Spannung.
* **Prüfung:** Messen Sie mit einem Multimeter die Spannung direkt am VCC-Pin des Sensors, während das Board nur an der Batterie hängt. Dort müssen ca. 3.3V anliegen.

### 2. Pull-up-Widerstand (Signalqualität)
* OneWire benötigt einen Pull-up-Widerstand zwischen DATA und VCC.
* Im Batteriebetrieb ist die Spannung oft etwas niedriger oder instabiler. Falls 4,7 kΩ nicht funktionieren, versuchen Sie einen **stärkeren Widerstand (2,2 kΩ)**.
* Der Code aktiviert zwar den internen Pull-up (`INPUT_PULLUP`), dieser ist aber oft zu schwach für längere Kabel oder niedrige Spannungen.

### 3. Spannungsstabilisierung im Code
* Wir haben im Code die Wartezeit nach dem Einschalten der Sensoren (Vext) auf **2 Sekunden** erhöht.
* Zusätzlich wird der OneWire-Bus vor dem Start kurz "geerdet" (Bus Reset), um undefinierte Zustände zu beheben.

### 4. Batterie-Messschaltung (VBAT_READ_CTL)
* Beim Heltec V3 muss GPIO 37 (`VBAT_READ_CTL`) auf **LOW** gesetzt werden, um die Batteriespannung zu messen. Im vorherigen Code stand dieser auf HIGH, was zu Fehlmessungen oder Störungen führen konnte. Dies wurde korrigiert.

### 5. Debugging über Serial Monitor
* Schließen Sie das Board an den PC an, öffnen Sie den Serial Monitor (115200 Baud) und drücken Sie den Reset-Knopf.
* Der Code gibt nun genau aus, wie viele Sensoren bei welchem Versuch gefunden wurden (z.B. "Versuch 1: 0 Sensoren gefunden").
* Wenn es über USB geht, aber über Batterie nicht, ist es fast immer ein Problem der **Verkabelung (5V statt 3.3V)** oder der **Signalstärke (Pull-up)**.

## Kompilierung
Um dieses Projekt mit `arduino-cli` zu kompilieren:
```bash
arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 Heltec-V3-LoRaWAN
```
