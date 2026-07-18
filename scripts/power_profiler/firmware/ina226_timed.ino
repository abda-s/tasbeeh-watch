// OPTIONAL firmware variant: INA226 -> Serial at a steady 100 Hz.
//
// Identical to the original sketch, but sensor reads + Serial prints are
// gated by `sampleInterval` instead of running as fast as loop() can go.
// A steady sample rate makes the Python profiler's charge/energy
// integration cleaner. NOT required -- the profiler works with the
// free-running sketch too.

#include <Wire.h>
#include "INA226.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
INA226 ina(0x40);

unsigned long previousSampleMillis = 0;
const long sampleInterval = 10;   // ms -> ~100 Hz power samples

unsigned long previousDisplayMillis = 0;
const long displayInterval = 100; // ms -> 10 Hz OLED refresh

float busVoltage_V = 0.0f;
float current_mA = 0.0f;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  if (!ina.begin() || !ina.isConnected()) {
    Serial.println("Failed to initialize INA226. Check wiring.");
    while (1);
  }

  int calError = ina.setMaxCurrentShunt(5.0, 0.01);
  if (calError != 0) {
    Serial.print("INA226 Calibration Error: ");
    Serial.println(calError);
    while (1) { delay(10); }
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  // YELLOW BAND label (top 16 px)
  display.setCursor(100, 0);
  display.print("V");

  // BLUE BAND label (bottom 48 px)
  display.setCursor(100, 32);
  display.print("mA");

  display.display();
}

void loop() {
  unsigned long now = millis();

  // ---- steady-rate sensor sampling + serial output ----
  if (now - previousSampleMillis >= sampleInterval) {
    previousSampleMillis = now;

    busVoltage_V = ina.getBusVoltage();
    current_mA = ina.getCurrent() * 1000.0;

    Serial.print(busVoltage_V, 3);
    Serial.print(",");
    Serial.println(current_mA, 2);
  }

  // ---- slower OLED refresh using the latest sample ----
  if (now - previousDisplayMillis >= displayInterval) {
    previousDisplayMillis = now;

    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);

    // Voltage numbers (yellow band)
    display.fillRect(0, 0, 95, 16, SSD1306_BLACK);
    display.setCursor(0, 0);
    display.print(busVoltage_V, 2);

    // Current numbers (blue band)
    display.fillRect(0, 32, 95, 16, SSD1306_BLACK);
    display.setCursor(0, 32);
    display.print(current_mA, 1);

    display.display();
  }
}
