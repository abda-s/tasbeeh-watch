# Hardware

> Board, display, touch, battery and charger details.
>
> [← Documentation index](README.md) · [Project README](../README.md)

---

## Hardware

- **Board**: Waveshare ESP32-S3-Touch-LCD-1.28
- **MCU**: ESP32-S3 (240 MHz capable; firmware runs 80 MHz awake, real `esp_light_sleep_start()` — CPU halted, not just downclocked — while the screen is off, 320KB SRAM, 16MB Flash, 2MB PSRAM)
- **Display**: 1.28" round TFT, 240×240, GC9A01, SPI (40 MHz)
- **Touch**: CST816S capacitive, I2C (pins 6/7)
- **Battery**: ADC pin 1 (GPIO1), 200K+100K voltage divider (3:1 ratio), ETA6098 charger
  (silkscreened on the schematic as "ETA6098" — earlier revisions of this doc had a
  typo, "ETA6096"). No battery-side undervoltage protection: the charger only
  handles charging, and the VBAT→VSYS path when unplugged is a plain always-on
  P-FET (Q2/AO3401) with no protection logic — see [Deep Sleep & Battery Lockdown](deep-sleep-and-lockdown.md).

---

## Battery Monitoring

- **Pin**: GPIO1 (ADC1_CH0) via 200K + 100K voltage divider (ratio 3:1)
- **Method**: `analogReadMilliVolts()` — uses ESP32-S3 factory ADC calibration
- **Formula**: V_bat = V_adc × 3.0, mapped 3.5V→0% to 4.15V→100%
- **Update**: Every 5 seconds, displayed at top-left of home screen
- **Color**: Red below 20%, grey otherwise
