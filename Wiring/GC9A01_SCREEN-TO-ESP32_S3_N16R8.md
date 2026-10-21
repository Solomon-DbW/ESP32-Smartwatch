# Wiring GC9A01 Screen to ESP32-S3-N16R8

P.S. on a breadboard, connect 3V3 to positive rail and GND to negative rail, connect display directly to breadboard on column 'e', make remaining ESP32 connections in column 'd' with the same row as respective display pins.

| GC9A01 Display Pin | ESP32-S3-N16R8 Pin |
| :----------------  | :----------------- |
| VCC                | 3V3                |
| GND                | GND                |
| SCL                | GPIO 12            |
| SDA                | GPIO 11            |
| CS                 | GPIO 10            |
| DC                 | GPIO 9             |
| RST                | GPIO 8             |
