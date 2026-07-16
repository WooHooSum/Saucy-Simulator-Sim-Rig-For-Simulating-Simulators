#include <TFT_eSPI.h>

TFT_eSPI tft;

void setup() {
  tft.init();
  tft.fillScreen(TFT_BLACK);

  const int16_t width = tft.width();
  const int16_t height = tft.height();

  // Static native-orientation test. No setRotation() call anywhere.
  tft.drawRect(1, 1, width - 2, height - 2, TFT_WHITE);
  tft.fillCircle(width / 2, height / 2, 5, TFT_WHITE);
}

void loop() {
}
