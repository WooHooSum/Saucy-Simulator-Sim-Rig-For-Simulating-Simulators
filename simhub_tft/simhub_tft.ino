#include <TFT_eSPI.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

TFT_eSPI tft;

// SimHub packets:
// F;<rpm>;<speed-kmh>;<delta-seconds>\n     (60 Hz)
// S;<gear>;<last-lap>;<max-rpm>\n           (changes only)
constexpr size_t RX_BUFFER_SIZE = 96;
char rxBuffer[RX_BUFFER_SIZE];
size_t rxLength = 0;

int rpm = 0;
int speedKmh = 0;
float deltaSeconds = 0.0f;
int maxRpm = 8000;
char gear[4] = "N";
char lastLap[16] = "--:--.---";

bool fastDirty = true;
bool stateDirty = true;
bool serialConnected = false;
uint32_t lastPacketMs = 0;
uint32_t lastFastDrawMs = 0;

constexpr uint32_t FAST_DRAW_INTERVAL_MS = 33; // Render at ~30 FPS.
constexpr int16_t SCREEN_WIDTH = 320;
constexpr int16_t SCREEN_HEIGHT = 240;
constexpr int16_t RPM_SEGMENTS = 16;

// High-contrast dark dashboard palette (RGB565).
constexpr uint16_t DASH_BACKGROUND = 0x0000;
constexpr uint16_t DASH_PANEL = 0x0000;
constexpr uint16_t DASH_BORDER = 0xFFFF;
constexpr uint16_t DASH_MUTED_TEXT = 0xFFFF;

constexpr int16_t SHIFT_DOT_Y = 8;
constexpr int16_t SHIFT_DOT_RADIUS = 5;
constexpr int16_t SHIFT_DOT_START_X = 10;
constexpr int16_t SHIFT_DOT_SPACING = 20;

int lastDrawnRpm = -1;
int lastDrawnSpeed = -1;
int lastDrawnMaxRpm = -1;
int lastLitSegments = -1;
float lastDrawnDelta = NAN;
char lastDrawnGear[4] = "";
char lastDrawnLap[16] = "";

void drawLabel(const char *label, int16_t x, int16_t y) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(DASH_MUTED_TEXT, DASH_PANEL);
  tft.setTextSize(1);
  tft.drawString(label, x, y, 2);
}

void drawStaticLayout() {
  tft.fillScreen(DASH_BACKGROUND);

  // Keep every dashboard region pure black.
  tft.fillRect(1, 17, 318, 26, DASH_PANEL);
  tft.fillRect(1, 44, 103, 195, DASH_PANEL);
  tft.fillRect(105, 44, 214, 74, DASH_PANEL);
  tft.fillRect(105, 119, 214, 60, DASH_PANEL);
  tft.fillRect(105, 180, 214, 59, DASH_PANEL);

  tft.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, DASH_BORDER);
  tft.drawFastHLine(0, 16, SCREEN_WIDTH, DASH_BORDER);
  tft.drawFastHLine(0, 43, SCREEN_WIDTH, DASH_BORDER);
  tft.drawFastVLine(104, 43, SCREEN_HEIGHT - 43, DASH_BORDER);
  tft.drawFastHLine(104, 118, SCREEN_WIDTH - 104, DASH_BORDER);
  tft.drawFastHLine(104, 179, SCREEN_WIDTH - 104, DASH_BORDER);

  drawLabel("GEAR", 52, 50);
  drawLabel("SPEED  KM/H", 212, 50);
  drawLabel("DELTA", 212, 125);
  drawLabel("LAST LAP", 212, 186);
}

void drawConnectionIndicator(bool connected) {
  tft.fillCircle(311, 29, 4, connected ? TFT_GREEN : TFT_RED);
}

void drawRpmBar() {
  int safeMax = maxRpm > 0 ? maxRpm : 8000;
  int lit = (long)constrain(rpm, 0, safeMax) * RPM_SEGMENTS / safeMax;
  if (rpm > 0 && lit == 0) lit = 1;

  if (lit != lastLitSegments || safeMax != lastDrawnMaxRpm) {
    for (int i = 0; i < RPM_SEGMENTS; ++i) {
      int16_t x = SHIFT_DOT_START_X + i * SHIFT_DOT_SPACING;
      if (i < lit) {
        uint16_t color = i < 8 ? TFT_GREEN : (i < 12 ? TFT_YELLOW : TFT_RED);
        tft.fillCircle(x, SHIFT_DOT_Y, SHIFT_DOT_RADIUS, color);
      } else {
        // Inactive shift lights stay visible as crisp white rings.
        tft.fillCircle(x, SHIFT_DOT_Y, SHIFT_DOT_RADIUS, DASH_BACKGROUND);
        tft.drawCircle(x, SHIFT_DOT_Y, SHIFT_DOT_RADIUS, TFT_WHITE);
      }
    }
    lastLitSegments = lit;
  }
}

void drawRpmNumber() {
  if (rpm == lastDrawnRpm && maxRpm == lastDrawnMaxRpm) return;

  char text[28];
  snprintf(text, sizeof(text), "RPM %d / %d", rpm, maxRpm);
  tft.fillRect(72, 19, 176, 21, DASH_PANEL);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, DASH_PANEL);
  tft.setTextSize(1);
  tft.drawString(text, 160, 29, 2);
  lastDrawnRpm = rpm;
  lastDrawnMaxRpm = maxRpm;
}

void drawSpeed() {
  if (speedKmh == lastDrawnSpeed) return;

  char text[8];
  snprintf(text, sizeof(text), "%d", speedKmh);
  tft.fillRect(145, 68, 135, 47, DASH_PANEL);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, DASH_PANEL);
  tft.setTextSize(1);
  tft.drawString(text, 212, 91, 6);
  lastDrawnSpeed = speedKmh;
}

void drawDelta() {
  // Limit screen churn to the precision actually displayed.
  float rounded = roundf(deltaSeconds * 1000.0f) / 1000.0f;
  if (!isnan(lastDrawnDelta) && rounded == lastDrawnDelta) return;

  char text[16];
  snprintf(text, sizeof(text), "%+.3f", rounded);
  tft.fillRect(137, 143, 151, 31, DASH_PANEL);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, DASH_PANEL);
  tft.setTextSize(1);
  tft.drawString(text, 212, 158, 4);
  lastDrawnDelta = rounded;
}

void drawGear() {
  if (strcmp(gear, lastDrawnGear) == 0) return;

  tft.fillRect(8, 69, 88, 163, DASH_PANEL);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, DASH_PANEL);
  tft.setTextSize(3);
  tft.drawString(gear, 52, 137, 4);
  tft.setTextSize(1);
  snprintf(lastDrawnGear, sizeof(lastDrawnGear), "%s", gear);
}

void drawLastLap() {
  if (strcmp(lastLap, lastDrawnLap) == 0) return;

  tft.fillRect(126, 202, 172, 31, DASH_PANEL);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, DASH_PANEL);
  tft.setTextSize(1);
  tft.drawString(lastLap, 212, 217, 4);
  snprintf(lastDrawnLap, sizeof(lastDrawnLap), "%s", lastLap);
}

void renderFastData() {
  drawRpmBar();
  drawRpmNumber();
  drawSpeed();
  drawDelta();
  fastDirty = false;
}

void renderStateData() {
  drawGear();
  drawLastLap();
  // A changed maximum RPM must immediately rescale the bar and header.
  drawRpmBar();
  drawRpmNumber();
  stateDirty = false;
}

void parseFastPacket(char *savePtr) {
  char *rpmToken = strtok_r(nullptr, ";", &savePtr);
  char *speedToken = strtok_r(nullptr, ";", &savePtr);
  char *deltaToken = strtok_r(nullptr, ";", &savePtr);
  if (!rpmToken || !speedToken || !deltaToken) return;

  int newRpm = atoi(rpmToken);
  int newSpeed = atoi(speedToken);
  float newDelta = strtof(deltaToken, nullptr);
  if (!isfinite(newDelta)) return;

  rpm = constrain(newRpm, 0, 50000);
  speedKmh = constrain(newSpeed, 0, 999);
  deltaSeconds = constrain(newDelta, -999.0f, 999.0f);
  fastDirty = true;
}

void parseStatePacket(char *savePtr) {
  char *gearToken = strtok_r(nullptr, ";", &savePtr);
  char *lapToken = strtok_r(nullptr, ";", &savePtr);
  char *maxRpmToken = strtok_r(nullptr, ";", &savePtr);
  if (!gearToken || !lapToken || !maxRpmToken) return;

  snprintf(gear, sizeof(gear), "%s", gearToken);
  snprintf(lastLap, sizeof(lastLap), "%s", lapToken);
  int parsedMax = atoi(maxRpmToken);
  if (parsedMax > 0 && parsedMax <= 50000) maxRpm = parsedMax;
  stateDirty = true;
}

void parsePacket(char *packet) {
  char *savePtr = nullptr;
  char *type = strtok_r(packet, ";", &savePtr);
  if (!type) return;

  if (strcmp(type, "F") == 0) parseFastPacket(savePtr);
  else if (strcmp(type, "S") == 0) parseStatePacket(savePtr);
  else return;

  lastPacketMs = millis();
  if (!serialConnected) {
    serialConnected = true;
    drawConnectionIndicator(true);
  }
}

void receiveSerial() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n') {
      if (rxLength > 0) {
        rxBuffer[rxLength] = '\0';
        parsePacket(rxBuffer);
      }
      rxLength = 0;
    } else if (c != '\r') {
      if (rxLength < RX_BUFFER_SIZE - 1) {
        rxBuffer[rxLength++] = c;
      } else {
        // Drop an oversized/corrupt packet and resynchronize at the next LF.
        rxLength = 0;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.invertDisplay(true); // This panel variant needs INVON for true RGB565 colors.
  tft.setRotation(3); // Landscape rotated 180 degrees (upside down).
  drawStaticLayout();
  drawConnectionIndicator(false);
  renderFastData();
  renderStateData();
}

void loop() {
  receiveSerial();

  uint32_t now = millis();
  if (fastDirty && now - lastFastDrawMs >= FAST_DRAW_INTERVAL_MS) {
    lastFastDrawMs = now;
    renderFastData();
  }
  if (stateDirty) renderStateData();

  if (serialConnected && now - lastPacketMs > 1000) {
    serialConnected = false;
    drawConnectionIndicator(false);
  }
}
