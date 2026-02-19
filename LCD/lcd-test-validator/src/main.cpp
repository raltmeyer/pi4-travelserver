#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// ---- CORRECT PINOUT from board diagram ----
// Touch has its OWN SPI bus, separate from TFT!
#define TP_CLK 25
#define TP_CS 33
#define TP_DIN 32 // MOSI (data TO touch chip)
#define TP_OUT 39 // MISO (data FROM touch chip)
#define TP_IRQ 36

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);

// Raw SPI read from XPT2046
uint16_t touchReadChannel(uint8_t cmd) {
  touchSPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TP_CS, LOW);
  touchSPI.transfer(cmd);
  uint8_t hi = touchSPI.transfer(0);
  uint8_t lo = touchSPI.transfer(0);
  digitalWrite(TP_CS, HIGH);
  touchSPI.endTransaction();
  return ((hi << 8) | lo) >> 3;
}

void touchPowerDown() {
  touchSPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TP_CS, LOW);
  touchSPI.transfer(0x80);
  touchSPI.transfer(0);
  touchSPI.transfer(0);
  digitalWrite(TP_CS, HIGH);
  touchSPI.endTransaction();
}

void drawCrosshair(int x, int y, uint16_t color) {
  tft.drawLine(x - 8, y, x + 8, y, color);
  tft.drawLine(x, y - 8, x, y + 8, color);
  tft.drawCircle(x, y, 5, color);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[BOOT] Touch Calibration v6");

  // Init TFT
  tft.init();
  tft.setRotation(1); // Landscape 320x240
  tft.fillScreen(TFT_BLACK);

  // Draw calibration targets at known positions
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(80, 110);
  tft.println("Touch anywhere - watch raw coords");

  // Draw crosshairs at corners and center
  drawCrosshair(20, 20, TFT_RED);      // Top-left
  drawCrosshair(300, 20, TFT_RED);     // Top-right
  drawCrosshair(160, 120, TFT_YELLOW); // Center
  drawCrosshair(20, 220, TFT_RED);     // Bottom-left
  drawCrosshair(300, 220, TFT_RED);    // Bottom-right

  // Init Touch
  pinMode(TP_CS, OUTPUT);
  digitalWrite(TP_CS, HIGH);
  pinMode(TP_IRQ, INPUT);
  touchSPI.begin(TP_CLK, TP_OUT, TP_DIN, TP_CS);
  touchPowerDown();

  Serial.println("[BOOT] Ready. Touch the crosshairs!");
  Serial.println("---");
}

void loop() {
  if (digitalRead(TP_IRQ) == LOW) {
    uint16_t z1 = touchReadChannel(0xB1);
    uint16_t z2 = touchReadChannel(0xC1);
    int z = z1 + 4095 - z2;

    if (z > 400) {
      // Read raw X and Y (XPT2046 commands: X=0xD0, Y=0x90)
      uint16_t rawX = touchReadChannel(0xD1);
      uint16_t rawY = touchReadChannel(0x91);

      // Print raw values
      Serial.print("[TOUCH] rawX=");
      Serial.print(rawX);
      Serial.print(" rawY=");
      Serial.print(rawY);
      Serial.print(" Z=");
      Serial.println(z);

      // Map with swapped axes (common on CYD boards)
      // Try: screen X from rawY, screen Y from rawX (swapped + inverted)
      int screenX = map(rawY, 200, 3800, 0, 320);
      int screenY = map(rawX, 300, 3700, 0, 240);
      screenX = constrain(screenX, 0, 319);
      screenY = constrain(screenY, 0, 239);

      // Draw dot + show raw coords on screen
      tft.fillCircle(screenX, screenY, 3, TFT_WHITE);

      // Show raw values at bottom of screen
      tft.fillRect(0, 228, 320, 12, TFT_BLACK);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setTextSize(1);
      tft.setCursor(5, 230);
      char buf[60];
      snprintf(buf, sizeof(buf), "Raw: X=%d Y=%d  Screen: %d,%d", rawX, rawY,
               screenX, screenY);
      tft.print(buf);
    }

    touchPowerDown();
    delay(50);
  }
}
