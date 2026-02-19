#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// =============================================
// PIN CONFIGURATION (ESP32-2432S028)
// =============================================
#define TP_CLK 25
#define TP_CS 33
#define TP_DIN 32
#define TP_OUT 39
#define TP_IRQ 36

// =============================================
// COLORS
// =============================================
#define COLOR_BG 0x1082
#define COLOR_TEXT TFT_WHITE
#define COLOR_DIM 0x8410
#define COLOR_ACCENT 0x04FF
#define COLOR_BAR_BG 0x2945
#define COLOR_CPU 0x07E0
#define COLOR_RAM 0x051F
#define COLOR_DISK 0xFBE0
#define COLOR_TEMP_OK 0x07E0
#define COLOR_TEMP_WARN 0xFD20
#define COLOR_TEMP_HOT 0xF800
#define COLOR_TAB_ACTIVE COLOR_ACCENT
#define COLOR_TAB_INACTIVE 0x3186

#define COLOR_BTN_ORANGE 0xFC00
#define COLOR_BTN_RED 0xD000
#define COLOR_BTN_DKRED 0x8000
#define COLOR_BTN_GREEN 0x0640
#define COLOR_BTN_YELLOW 0xF5A0

// =============================================
// LAYOUT
// =============================================
#define SCREEN_W 320
#define SCREEN_H 240
#define TAB_BAR_H 36
#define CONTENT_H (SCREEN_H - TAB_BAR_H)

// Font shortcuts: Font 2 = 16px proportional, Font 4 = 26px proportional
#define FONT_SM 2
#define FONT_LG 4

// =============================================
// GLOBALS
// =============================================
TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);

int currentTab = 0;

// System stats
float cpuPercent = 0;
int ramTotal = 0, ramUsed = 0;
float ramPercent = 0;
int diskTotal = 0, diskUsed = 0;
float diskPercent = 0;
float temperature = 0;
String ipWlan0 = "N/A";
String ipWlan1 = "N/A";
int uptimeSecs = 0;
bool dataReceived = false;

// Touch
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE = 300;

// Confirmation dialog
int pendingButtonIdx = -1; // -1 = no dialog, 0-6 = which button
unsigned long confirmFeedbackTime = 0;
bool showSentFeedback = false;

// Serial
String serialBuffer = "";

// =============================================
// TOUCH
// =============================================
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

bool getTouch(int &x, int &y) {
  if (digitalRead(TP_IRQ) != LOW)
    return false;
  uint16_t z1 = touchReadChannel(0xB1);
  uint16_t z2 = touchReadChannel(0xC1);
  int z = z1 + 4095 - z2;
  if (z > 400) {
    uint16_t rawX = touchReadChannel(0xD1);
    uint16_t rawY = touchReadChannel(0x91);
    touchPowerDown();
    x = map(rawY, 200, 3800, 0, 320);
    y = map(rawX, 300, 3700, 0, 240);
    x = constrain(x, 0, 319);
    y = constrain(y, 0, 239);
    return true;
  }
  touchPowerDown();
  return false;
}

// =============================================
// DRAWING HELPERS
// =============================================
void drawProgressBar(int x, int y, int w, int h, float percent,
                     uint16_t barColor) {
  tft.fillRoundRect(x, y, w, h, 4, COLOR_BAR_BG);
  int fillW = (int)((w - 2) * (percent / 100.0));
  if (fillW > 0) {
    tft.fillRoundRect(x + 1, y + 1, fillW, h - 2, 3, barColor);
  }
}

// Draw centered text button using Font 2
void drawButton(int x, int y, int w, int h, const char *label, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 6, color);
  tft.setTextFont(FONT_LG);
  tft.setTextSize(1);
  int tw = tft.textWidth(label);
  int th = tft.fontHeight();
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(x + (w - tw) / 2, y + (h - th) / 2);
  tft.print(label);
}

bool isButtonPressed(int touchX, int touchY, int bx, int by, int bw, int bh) {
  return (touchX >= bx && touchX <= bx + bw && touchY >= by &&
          touchY <= by + bh);
}

// =============================================
// TAB BAR
// =============================================
void drawTabBar() {
  int tabW = SCREEN_W / 2;
  int tabY = SCREEN_H - TAB_BAR_H;

  // Status tab
  tft.fillRect(0, tabY, tabW, TAB_BAR_H,
               currentTab == 0 ? COLOR_TAB_ACTIVE : COLOR_TAB_INACTIVE);
  tft.setTextFont(FONT_LG);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  int tw = tft.textWidth("STATUS");
  tft.setCursor((tabW - tw) / 2, tabY + 6);
  tft.print("STATUS");

  // Controls tab
  tft.fillRect(tabW, tabY, tabW, TAB_BAR_H,
               currentTab == 1 ? COLOR_TAB_ACTIVE : COLOR_TAB_INACTIVE);
  tw = tft.textWidth("CONTROL");
  tft.setCursor(tabW + (tabW - tw) / 2, tabY + 6);
  tft.print("CONTROL");

  // Separator
  tft.drawFastVLine(tabW, tabY, TAB_BAR_H, COLOR_BG);
}

// =============================================
// STATUS TAB
// =============================================
void drawStatusTab() {
  tft.fillRect(0, 0, SCREEN_W, CONTENT_H, COLOR_BG);

  if (!dataReceived) {
    tft.setTextFont(FONT_LG);
    tft.setTextColor(COLOR_DIM);
    int tw = tft.textWidth("Waiting for Pi...");
    tft.setCursor((SCREEN_W - tw) / 2, 80);
    tft.print("Waiting for Pi...");
    return;
  }

  char buf[32];
  int y = 4;
  int labelX = 4;
  int barX = 52;
  int barW = 200;
  int barH = 16;
  int valX = 258;

  // --- CPU ---
  tft.setTextFont(FONT_SM);
  tft.setTextColor(COLOR_CPU);
  tft.setCursor(labelX, y);
  tft.print("CPU");
  drawProgressBar(barX, y, barW, barH, cpuPercent, COLOR_CPU);
  tft.setTextColor(COLOR_TEXT);
  snprintf(buf, sizeof(buf), "%.0f%%", cpuPercent);
  tft.setCursor(valX, y);
  tft.print(buf);
  y += 24;

  // --- RAM ---
  tft.setTextColor(COLOR_RAM);
  tft.setCursor(labelX, y);
  tft.print("RAM");
  drawProgressBar(barX, y, barW, barH, ramPercent, COLOR_RAM);
  tft.setTextColor(COLOR_TEXT);
  snprintf(buf, sizeof(buf), "%.0f%%", ramPercent);
  tft.setCursor(valX, y);
  tft.print(buf);
  y += 16;
  tft.setTextColor(COLOR_DIM);
  tft.setCursor(barX, y);
  snprintf(buf, sizeof(buf), "%d / %d MB", ramUsed, ramTotal);
  tft.print(buf);
  y += 20;

  // --- DISK ---
  tft.setTextColor(COLOR_DISK);
  tft.setCursor(labelX, y);
  tft.print("DSK");
  drawProgressBar(barX, y, barW, barH, diskPercent, COLOR_DISK);
  tft.setTextColor(COLOR_TEXT);
  snprintf(buf, sizeof(buf), "%.0f%%", diskPercent);
  tft.setCursor(valX, y);
  tft.print(buf);
  y += 16;
  tft.setTextColor(COLOR_DIM);
  tft.setCursor(barX, y);
  snprintf(buf, sizeof(buf), "%d / %d GB", diskUsed, diskTotal);
  tft.print(buf);
  y += 22;

  // --- TEMP ---
  uint16_t tempColor = COLOR_TEMP_OK;
  if (temperature > 70)
    tempColor = COLOR_TEMP_HOT;
  else if (temperature > 55)
    tempColor = COLOR_TEMP_WARN;

  tft.setTextFont(FONT_LG);
  tft.setTextColor(tempColor);
  snprintf(buf, sizeof(buf), "%.1f'C", temperature);
  tft.setCursor(labelX, y);
  tft.print(buf);

  // Uptime on same line, right-aligned
  int hours = uptimeSecs / 3600;
  int mins = (uptimeSecs % 3600) / 60;
  snprintf(buf, sizeof(buf), "UP %dh%dm", hours, mins);
  tft.setTextColor(COLOR_DIM);
  int tw = tft.textWidth(buf);
  tft.setCursor(SCREEN_W - tw - 4, y);
  tft.print(buf);
  y += 30;

  // --- Divider ---
  tft.drawFastHLine(4, y, SCREEN_W - 8, COLOR_TAB_INACTIVE);
  y += 6;

  // --- Network ---
  tft.setTextFont(FONT_SM);
  tft.setTextColor(COLOR_ACCENT);
  tft.setCursor(labelX, y);
  tft.print("AP ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(ipWlan0);

  tft.setTextColor(COLOR_ACCENT);
  tft.setCursor(SCREEN_W / 2, y);
  tft.print("WAN ");
  tft.setTextColor(COLOR_TEXT);
  tft.print(ipWlan1);
}

// =============================================
// CONTROLS TAB
// =============================================
struct Button {
  int x, y, w, h;
  const char *label;
  const char *action;
  uint16_t color;
};

const int BTN_W = 148;
const int BTN_H = 42;
const int BTN_PAD = 8;
const int BTN_X1 = 4;
const int BTN_X2 = 168;

Button buttons[] = {
    {BTN_X1, 6, BTN_W, BTN_H, "Reset Net", "reset_network", COLOR_BTN_ORANGE},
    {BTN_X2, 6, BTN_W, BTN_H, "FW Strict", "fw_strict", COLOR_BTN_RED},
    {BTN_X1, 6 + (BTN_H + BTN_PAD), BTN_W, BTN_H, "FW Maint", "fw_maint",
     COLOR_BTN_YELLOW},
    {BTN_X2, 6 + (BTN_H + BTN_PAD), BTN_W, BTN_H, "Start SMB", "start_smb",
     COLOR_BTN_GREEN},
    {BTN_X1, 6 + 2 * (BTN_H + BTN_PAD), BTN_W, BTN_H, "Stop SMB", "stop_smb",
     COLOR_BTN_RED},
    {BTN_X2, 6 + 2 * (BTN_H + BTN_PAD), BTN_W, BTN_H, "Reboot", "reboot",
     COLOR_BTN_ORANGE},
    {BTN_X1, 6 + 3 * (BTN_H + BTN_PAD), BTN_W, BTN_H, "Shutdown", "shutdown",
     COLOR_BTN_DKRED},
};
const int NUM_BUTTONS = 7;

void drawControlsTab() {
  tft.fillRect(0, 0, SCREEN_W, CONTENT_H, COLOR_BG);
  for (int i = 0; i < NUM_BUTTONS; i++) {
    drawButton(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h,
               buttons[i].label, buttons[i].color);
  }
}

void sendCommand(const char *action) {
  JsonDocument doc;
  doc["action"] = action;
  String json;
  serializeJson(doc, json);
  Serial.println(json);
}

// =============================================
// CONFIRMATION DIALOG
// =============================================
#define DIALOG_W 280
#define DIALOG_H 140
#define DIALOG_X ((SCREEN_W - DIALOG_W) / 2)
#define DIALOG_Y ((SCREEN_H - DIALOG_H) / 2 - 10)
#define DBTN_W 110
#define DBTN_H 40
#define DBTN_Y (DIALOG_Y + DIALOG_H - DBTN_H - 12)
#define DBTN_YES_X (DIALOG_X + 20)
#define DBTN_NO_X (DIALOG_X + DIALOG_W - DBTN_W - 20)

void drawConfirmDialog(const char *label) {
  // Dim background
  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, 0x0841);

  // Dialog box
  tft.fillRoundRect(DIALOG_X, DIALOG_Y, DIALOG_W, DIALOG_H, 10, 0x2945);
  tft.drawRoundRect(DIALOG_X, DIALOG_Y, DIALOG_W, DIALOG_H, 10, COLOR_ACCENT);

  // Title: "Confirm?"
  tft.setTextFont(FONT_LG);
  tft.setTextColor(COLOR_TEXT);
  char title[40];
  snprintf(title, sizeof(title), "Confirm?");
  int tw = tft.textWidth(title);
  tft.setCursor(DIALOG_X + (DIALOG_W - tw) / 2, DIALOG_Y + 14);
  tft.print(title);

  // Action name
  tft.setTextFont(FONT_LG);
  tft.setTextColor(COLOR_ACCENT);
  tw = tft.textWidth(label);
  tft.setCursor(DIALOG_X + (DIALOG_W - tw) / 2, DIALOG_Y + 48);
  tft.print(label);

  // YES button
  drawButton(DBTN_YES_X, DBTN_Y, DBTN_W, DBTN_H, "YES", COLOR_BTN_GREEN);
  // NO button
  drawButton(DBTN_NO_X, DBTN_Y, DBTN_W, DBTN_H, "NO", COLOR_BTN_RED);
}

void showSentOverlay() {
  int boxW = 180;
  int boxH = 36;
  int boxX = (SCREEN_W - boxW) / 2;
  int boxY = (SCREEN_H - boxH) / 2;
  tft.fillRoundRect(boxX, boxY, boxW, boxH, 8, COLOR_ACCENT);
  tft.setTextFont(FONT_LG);
  tft.setTextColor(TFT_BLACK);
  int tw = tft.textWidth("Sent!");
  tft.setCursor(boxX + (boxW - tw) / 2, boxY + 6);
  tft.print("Sent!");
}

// =============================================
// JSON PARSING
// =============================================
void parseSerialData(const String &line) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err)
    return;

  cpuPercent = doc["cpu"] | 0.0f;
  JsonObject ram = doc["ram"];
  if (ram) {
    ramTotal = ram["total"] | 0;
    ramUsed = ram["used"] | 0;
    ramPercent = ram["percent"] | 0.0f;
  }
  JsonObject disk = doc["disk"];
  if (disk) {
    diskTotal = disk["total"] | 0;
    diskUsed = disk["used"] | 0;
    diskPercent = disk["percent"] | 0.0f;
  }
  temperature = doc["temp"] | 0.0f;
  JsonObject net = doc["net"];
  if (net) {
    ipWlan0 = net["wlan0"] | "N/A";
    ipWlan1 = net["wlan1"] | "N/A";
  }
  uptimeSecs = doc["uptime"] | 0;
  dataReceived = true;
}

// =============================================
// SETUP
// =============================================
void setup() {
  Serial.begin(115200);
  delay(500);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);

  pinMode(TP_CS, OUTPUT);
  digitalWrite(TP_CS, HIGH);
  pinMode(TP_IRQ, INPUT);
  touchSPI.begin(TP_CLK, TP_OUT, TP_DIN, TP_CS);
  touchPowerDown();

  drawTabBar();
  drawStatusTab();
}

// =============================================
// LOOP
// =============================================
void loop() {
  // Serial read
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if (serialBuffer.length() > 0 && serialBuffer.charAt(0) == '{') {
        parseSerialData(serialBuffer);
        if (currentTab == 0)
          drawStatusTab();
      }
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
      if (serialBuffer.length() > 512)
        serialBuffer = "";
    }
  }

  // Touch
  int tx, ty;
  if (getTouch(tx, ty) && (millis() - lastTouchTime > TOUCH_DEBOUNCE)) {
    lastTouchTime = millis();

    // --- Confirmation dialog is active ---
    if (pendingButtonIdx >= 0) {
      // YES button
      if (isButtonPressed(tx, ty, DBTN_YES_X, DBTN_Y, DBTN_W, DBTN_H)) {
        sendCommand(buttons[pendingButtonIdx].action);
        pendingButtonIdx = -1;
        showSentOverlay();
        showSentFeedback = true;
        confirmFeedbackTime = millis();
      }
      // NO button
      else if (isButtonPressed(tx, ty, DBTN_NO_X, DBTN_Y, DBTN_W, DBTN_H)) {
        pendingButtonIdx = -1;
        drawTabBar();
        drawControlsTab();
      }
      // Tap outside dialog = cancel
      else if (tx < DIALOG_X || tx > DIALOG_X + DIALOG_W || ty < DIALOG_Y ||
               ty > DIALOG_Y + DIALOG_H) {
        pendingButtonIdx = -1;
        drawTabBar();
        drawControlsTab();
      }
    }
    // --- Normal UI ---
    else {
      int tabY = SCREEN_H - TAB_BAR_H;

      if (ty >= tabY) {
        int newTab = (tx < SCREEN_W / 2) ? 0 : 1;
        if (newTab != currentTab) {
          currentTab = newTab;
          drawTabBar();
          if (currentTab == 0)
            drawStatusTab();
          else
            drawControlsTab();
        }
      } else if (currentTab == 1) {
        for (int i = 0; i < NUM_BUTTONS; i++) {
          if (isButtonPressed(tx, ty, buttons[i].x, buttons[i].y, buttons[i].w,
                              buttons[i].h)) {
            // Flash button
            tft.fillRoundRect(buttons[i].x, buttons[i].y, buttons[i].w,
                              buttons[i].h, 6, TFT_WHITE);
            delay(80);
            drawButton(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h,
                       buttons[i].label, buttons[i].color);
            // Show confirmation dialog
            pendingButtonIdx = i;
            drawConfirmDialog(buttons[i].label);
            break;
          }
        }
      }
    }
  }

  // Clear "Sent!" feedback after 1.5s
  if (showSentFeedback && millis() - confirmFeedbackTime > 1500) {
    showSentFeedback = false;
    drawTabBar();
    drawControlsTab();
  }
}
