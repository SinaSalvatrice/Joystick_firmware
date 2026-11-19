# Joystick Editor für ESP32-C3

Unter Windows startet `desktop-app\launch.cmd` die App aus dem Repository.
Beim ersten Start braucht sie Python 3 mit Tkinter und richtet `pyserial` in
einer eigenen Umgebung im Benutzerprofil ein.

| Signal | GPIO am ESP32-C3 |
| --- | --- |
| Poti X | GPIO0 |
| Poti Y | GPIO1 |
| Taste 1 | GPIO2 |
| Taste 2 | GPIO3 |
| Stick-Klick | GPIO4 |
| NeoPixel-DIN | GPIO5, drei WS2812 |

Der Sketch `firmware/joystick_c3/joystick_c3.ino` lässt sich mit dem
ESP32-Arduino-Core für das jeweilige ESP32-C3-Board und der Arduino-Bibliothek
**Adafruit NeoPixel** kompilieren. Bei Verbindung über den nativen USB-Port
**USB CDC On Boot** aktivieren. Bei Verwendung der UART-USB-Brücke
**USB CDC On Boot** deaktivieren, damit `Serial` über UART läuft.
Der Sketch nutzt 115200 Baud. Beim Einschalten oder Reset die Taste an
GPIO2 loslassen, da dieser Pin beim ESP32-C3 zur Boot-Konfiguration gehört.

1. In der App den USB-COM-Port des Joysticks auswählen und verbinden.
2. Im **Tester** die Achsen bewegen und die Tasten drücken. Die Grafik zeigt
   die Rohwerte. Die Ereignisliste enthält Auslenkungen und Tastendrücke.
3. Unter **Belegung** den Stick loslassen und **Aktuelle Position als Mitte**
   wählen. Totzone, Achsenrichtung, Tastenkürzel und Wiederholung sind
   einstellbar. Die Profile lassen sich speichern und als JSON austauschen.
4. Unter **NeoPixels** Farbe, Helligkeit und Effekt wählen. Die drei
   Sichttests schalten jeweils ein Pixel für 1,2 Sekunden ein.

Die Tastaturausgabe erfolgt in Windows über die laufende App und wird
ausdrücklich eingeschaltet. Beim Wechsel in den Tester wird sie deaktiviert.
Die Firmware überträgt Eingaben über den COM-Port. Ein älterer ESP32-C3-Sketch
mit Texttelemetrie lässt sich ebenfalls im Tester lesen. Die aktuelle
Firmware ist für die Tastaturausgabe und die LED-Steuerung erforderlich.

Das RP2040-QMK-Projekt im selben Repo verwendet analoge Eingänge an GP26
und GP27. Die hier angegebene GPIO0/GPIO1-Belegung gehört zur ESP32-C3-Variante.
