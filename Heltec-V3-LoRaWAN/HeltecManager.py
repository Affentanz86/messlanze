import serial
import serial.tools.list_ports
import customtkinter as ctk
import threading
import time

class HeltecManager(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title("Heltec V3 LoRa Manager v1.5")
        self.geometry("900x980")
        self.ser = None
        self.sensors = []

        # Tabs for better organization
        self.tabview = ctk.CTkTabview(self)
        self.tabview.pack(fill="both", expand=True, padx=10, pady=10)

        self.tab_config = self.tabview.add("Konfiguration")
        self.tab_guide = self.tabview.add("Anleitung & Hilfe")

        self.setup_config_tab()
        self.setup_guide_tab()

    def setup_config_tab(self):
        # --- UI Setup ---
        header_f = ctk.CTkFrame(self.tab_config, fg_color="transparent")
        header_f.pack(fill="x", padx=20, pady=10)

        ctk.CTkLabel(header_f, text="Heltec V3 LoRaWAN Manager", font=("Arial", 22, "bold")).pack(side="left")

        # Hilfe Button im Header
        ctk.CTkButton(header_f, text="? HILFE", width=80, fg_color="gray30", command=lambda: self.tabview.set("Anleitung & Hilfe")).pack(side="right", padx=10)

        self.bat_label = ctk.CTkLabel(header_f, text="Bat: -- V", font=("Arial", 14, "bold"), text_color="gray")
        self.bat_label.pack(side="right", padx=10)

        # Port Auswahl
        port_f = ctk.CTkFrame(self.tab_config)
        port_f.pack(fill="x", padx=20, pady=5)
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_var = ctk.StringVar(value=ports[0] if ports else "Kein Port")
        self.port_menu = ctk.CTkOptionMenu(port_f, variable=self.port_var, values=ports if ports else ["Kein Port"])
        self.port_menu.pack(side="left", padx=10, pady=10)

        self.btn_conn = ctk.CTkButton(port_f, text="Verbinden", command=self.toggle_connect, fg_color="blue")
        self.btn_conn.pack(side="left", padx=5)

        # Eingabefelder
        self.deveui = self.create_input(self.tab_config, "DevEUI (Hex):")
        self.appeui = self.create_input(self.tab_config, "AppEUI/JoinEUI (Hex):")
        self.appkey = self.create_input(self.tab_config, "AppKey (Hex):")
        self.devname = self.create_input(self.tab_config, "Gerätename (OLED):")
        self.interval = self.create_input(self.tab_config, "Intervall (Min):")
        self.disp_dur = self.create_input(self.tab_config, "Display Dauer (Sek):")

        self.hori_var = ctk.BooleanVar(value=True)
        ctk.CTkCheckBox(self.tab_config, text="Layout Horizontal (2x2)", variable=self.hori_var).pack(pady=5)

        self.scroll = ctk.CTkScrollableFrame(self.tab_config, height=220, label_text="DS18B20 Sensoren (Position festlegen)")
        self.scroll.pack(fill="both", padx=20, pady=10)

        btn_f = ctk.CTkFrame(self.tab_config, fg_color="transparent")
        btn_f.pack(pady=10)
        ctk.CTkButton(btn_f, text="1. SENSOR SCAN", fg_color="orange", command=self.start_scan).pack(side="left", padx=5)
        ctk.CTkButton(btn_f, text="2. SPEICHERN & RESTART", fg_color="green", command=self.save_all, width=250).pack(side="left", padx=5)

        self.monitor = ctk.CTkTextbox(self.tab_config, height=150, fg_color="black", text_color="lime", font=("Consolas", 12))
        self.monitor.pack(fill="both", padx=20, pady=10)

    def setup_guide_tab(self):
        guide_text = """
=== HARDWARE ANLEITUNG (Heltec V3) ===

1. DS18B20 SENSOR ANSCHLUSS:
   - ROT (VCC):  An 3.3V Pin anschließen (NICHT 5V/VBus, da diese bei Akkubetrieb aus sind!)
   - SCHWARZ (GND): An GND Pin anschließen.
   - GELB (DATA): An GPIO 4 anschließen.
   - WICHTIG: Ein 4.7k Ohm Widerstand zwischen VCC (3.3V) und DATA (GPIO 4) ist zwingend erforderlich!

2. AKKU-MESSUNG:
   - Die Messung erfolgt intern über GPIO 1.
   - Der Jumper/Schalter für Vext (GPIO 36) wird automatisch gesteuert.

=== BEDIENUNGSANLEITUNG ===

Schritt 1: Board vorbereiten
- Halten Sie die 'PRG' Taste am Heltec Board gedrückt und drücken Sie kurz 'RST'.
- Halten Sie 'PRG' gedrückt, bis 'MODUS: SETUP' auf dem Display erscheint.

Schritt 2: Verbindung
- Wählen Sie den COM-Port aus und klicken Sie auf 'Verbinden'.
- Die aktuellen Einstellungen vom Board werden automatisch geladen.

Schritt 3: Sensoren scannen
- Klicken Sie auf '1. SENSOR SCAN'. Das Board sucht alle angeschlossenen DS18B20.
- Die gefundenen IDs (ROMs) erscheinen in der Liste.
- WICHTIG: Geben Sie in das kleine Feld rechts die POSITION (1, 2, 3 oder 4) ein.
- S1, S2, S3, S4 entspricht der Reihenfolge im LoRa-Paket und auf dem Display.

Schritt 4: Speichern
- Geben Sie Ihre LoRaWAN Keys (DevEUI, AppEUI, AppKey) ein.
- Klicken Sie auf '2. SPEICHERN & RESTART'.
- Das Board speichert die Daten im NVS (Flash) und startet im Normalmodus neu.

=== FEHLERBEHEBUNG ===
- Wenn Speichern nicht geht: Prüfen Sie, ob DevEUI/AppEUI 16 Zeichen und AppKey 32 Zeichen lang sind (nur 0-9, A-F).
- 'NVS Session ungültig gesetzt' ist normal beim Speichern. Es stellt sicher, dass das Board
  nach dem Neustart einen frischen OTAA Join durchführt.
"""
        self.guide_box = ctk.CTkTextbox(self.tab_guide, font=("Arial", 14), wrap="word")
        self.guide_box.pack(fill="both", expand=True, padx=20, pady=20)
        self.guide_box.insert("1.0", guide_text)
        self.guide_box.configure(state="disabled")

    def create_input(self, parent, label_text):
        f = ctk.CTkFrame(parent)
        f.pack(fill="x", padx=20, pady=2)
        ctk.CTkLabel(f, text=label_text, width=180, anchor="w").pack(side="left", padx=10)
        e = ctk.CTkEntry(f)
        e.pack(side="left", fill="x", expand=True, padx=10)
        return e

    def log(self, msg):
        self.monitor.insert("end", f"[{time.strftime('%H:%M:%S')}] {msg}\n")
        self.monitor.see("end")

    def toggle_connect(self):
        if self.ser and self.ser.is_open:
            self.ser.close(); self.ser = None
            self.btn_conn.configure(text="Verbinden", fg_color="blue")
            self.log("Getrennt.")
        else:
            try:
                self.ser = serial.Serial(self.port_var.get(), 115200, timeout=0.1)
                self.btn_conn.configure(text="Trennen", fg_color="red")
                self.log(f"Verbunden. Warte auf Daten vom Board...")
                threading.Thread(target=self.reader, daemon=True).start()
            except Exception as e: self.log(f"Fehler: {e}")

    def reader(self):
        while self.ser and self.ser.is_open:
            try:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if not line: continue

                if line.startswith("LORA_DATA|"):
                    p = line.split("|")
                    if len(p) >= 8:
                        self.deveui.delete(0, 'end'); self.deveui.insert(0, p[1])
                        self.appeui.delete(0, 'end'); self.appeui.insert(0, p[2])
                        self.appkey.delete(0, 'end'); self.appkey.insert(0, p[3])
                        self.interval.delete(0, 'end'); self.interval.insert(0, p[4])
                        self.devname.delete(0, 'end'); self.devname.insert(0, p[5])
                        self.hori_var.set(p[6] == "1")
                        self.disp_dur.delete(0, 'end'); self.disp_dur.insert(0, p[7])
                        self.log("Konfiguration empfangen.")

                elif "Battery:" in line:
                    # Format: Battery: 4.02V
                    val = line.split(":")[1].strip()
                    self.bat_label.configure(text=f"Bat: {val}", text_color="lime")

                elif line.startswith("ROM:"):
                    parts = line.split("|")
                    self.add_sensor_row(parts[0].split(":")[1], parts[1].split(":")[1])

                elif "OK:" in line: self.log(f"ERFOLG: {line}")
                elif "ERR:" in line: self.log(f"FEHLER: {line}")
                else: self.log(f"Board: {line}")
            except: break

    def add_sensor_row(self, rom, temp):
        f = ctk.CTkFrame(self.scroll)
        f.pack(fill="x", pady=2, padx=5)
        pos = len(self.sensors) + 1
        ctk.CTkLabel(f, text=f"S{pos}", width=30, text_color="orange", font=("Arial", 12, "bold")).pack(side="left", padx=5)
        ctk.CTkLabel(f, text=f"ID: {rom}", width=200, anchor="w").pack(side="left", padx=5)
        ctk.CTkLabel(f, text=f"{temp}°C", width=60).pack(side="left", padx=5)

        ctk.CTkLabel(f, text="Pos:", width=30).pack(side="left", padx=2)
        e = ctk.CTkEntry(f, width=40)
        e.insert(0, str(pos))
        e.pack(side="left", padx=5)
        self.sensors.append({"rom": rom, "entry": e})

    def start_scan(self):
        if not self.ser: return
        for w in self.scroll.winfo_children(): w.destroy()
        self.sensors = []; self.ser.write(b"SCAN\n")
        self.log("Starte Sensor-Scan...")

    def save_all(self):
        if not self.ser:
            self.log("Fehler: Nicht verbunden!")
            return

        # Validation
        dev = self.deveui.get().strip()
        app = self.appeui.get().strip()
        key = self.appkey.get().strip()

        if len(dev) != 16 or len(app) != 16 or len(key) != 32:
            self.log("FEHLER: Keys haben falsche Länge!")
            return

        self.sensors.sort(key=lambda x: int(x["entry"].get()) if x["entry"].get().isdigit() else 99)
        rom_list = ",".join([s["rom"] for s in self.sensors[:4]])
        # New Format: SAVE|DevEUI|AppEUI|AppKey|Interval|Name|Hori|Dur|ROM1,ROM2...
        cmd = f"SAVE|{dev}|{app}|{key}|{self.interval.get()}|{self.devname.get()}|{'1' if self.hori_var.get() else '0'}|{self.disp_dur.get()}|{rom_list}\n"

        self.log(f"Sende: SAVE|{dev[:4]}...|{rom_list[:20]}...")
        self.ser.write(cmd.encode())

if __name__ == "__main__":
    app = HeltecManager()
    app.mainloop()
