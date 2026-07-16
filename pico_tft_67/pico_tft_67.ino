#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Raspberry Pi Pico SPI0 wiring:
// TFT SCK/SCLK -> GP18 (physical pin 24)
// TFT MOSI/SDI -> GP19 (physical pin 25)
// TFT MISO/SDO -> GP16 (physical pin 21; used for controller identification)
// TFT CS       -> GP17 (physical pin 22)
// TFT DC/RS    -> GP15 (physical pin 20)
// TFT RST      -> GP14 (physical pin 19)
// TFT VCC/LED  -> 3V3, TFT GND -> GND
constexpr uint8_t TFT_CS  = 17;
constexpr uint8_t TFT_DC  = 15;
constexpr uint8_t TFT_RST = 14;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  // Standard Adafruit ILI9341 initialization. Set landscape once and leave it.
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  const char message[] = "6-7";
  int16_t x1, y1;
  uint16_t textWidth, textHeight;

  tft.setTextWrap(false);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(10);
  tft.getTextBounds(message, 0, 0, &x1, &y1, &textWidth, &textHeight);
  tft.setCursor((tft.width() - textWidth) / 2 - x1,
                (tft.height() - textHeight) / 2 - y1);
  tft.print(message);
}

void loop() {
  // The display retains its contents.
}
