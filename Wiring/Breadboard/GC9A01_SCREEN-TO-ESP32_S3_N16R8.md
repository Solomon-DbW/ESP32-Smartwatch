# Breadboard wiring GC9A01 Screen to ESP32-S3-N16R8

All of the GC9A01's pins are male and they are plugged directly into the breadboard

The wires for the GC9A01's pins (SCL, SDA, CD, DC & RST) are connected directly to the left of the display's male pins

| GC9A01 Display Pin | Breadboard               | ESP32-S3-N16R8 Pin |
| :----------------  | :----------------------- | :----------------- |
| VCC                | 20d -> Positive (+) Rail | 3V3                |
| GND                | 19e -> Negative (-) Rail | GND                |
| GC9A01 all pins    | 14e -> 20e               |                    |---------> Plugged directly into the breadboard
| SCL                | 18d                      | GPIO 12            | ---|
| SDA                | 17d                      | GPIO 11            |    |
| CS                 | 15d                      | GPIO 10            |    |----> These wires are connected directly to the left of the display's male pins, which are plugged directly into the breadboard
| DC                 | 16d                      | GPIO 9             |    |
| RST                | 14d                      | GPIO 8             | ---|
