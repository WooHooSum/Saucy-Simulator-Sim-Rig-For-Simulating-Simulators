#include <Arduino.h>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error This firmware requires an ESP32-S3 with native USB OTG.
#endif

#ifndef ARDUINO_USB_MODE
#error Select an ESP32-S3 board with native USB support.
#elif ARDUINO_USB_MODE != 0
#error Set Tools > USB Mode to "USB-OTG (TinyUSB)".
#endif

#if ARDUINO_USB_CDC_ON_BOOT
#error Set Tools > USB CDC On Boot to "Disabled" for the dedicated FFB HID interface.
#endif

#include <cinttypes>
#include <cstdio>

#include "USB.h"
#include "board_config.hpp"
#include "config_store.hpp"
#include "control_core.hpp"
#include "driver/gpio.h"
#include "ffb_hid_device.hpp"
#include "shared_state.hpp"
#include "usb_pid.hpp"

namespace {

constexpr std::uint16_t kPrivateTestVid = 0x1209;
constexpr std::uint16_t kPrivateTestPid = 0x0001;

RuntimeShared g_shared;
Configuration g_storage;

void force_motor_gpio_safe() {
  gpio_config_t output{};
  output.pin_bit_mask = (1ULL << board::kSharedPwm) |
                        (1ULL << board::kDirection1) |
                        (1ULL << board::kDirection2);
  output.mode = GPIO_MODE_OUTPUT;
  output.pull_down_en = GPIO_PULLDOWN_ENABLE;
  output.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&output);
  gpio_set_level(board::kSharedPwm, 0);
  gpio_set_level(board::kDirection1, 0);
  gpio_set_level(board::kDirection2, 0);
}

void usb_event(void*, esp_event_base_t event_base, std::int32_t event_id, void*) {
  if (event_base != ARDUINO_USB_EVENTS) return;
  switch (event_id) {
    case ARDUINO_USB_STARTED_EVENT:
      usb_ffb::mounted(true);
      usb_ffb::suspended(false);
      break;
    case ARDUINO_USB_STOPPED_EVENT:
      usb_ffb::mounted(false);
      break;
    case ARDUINO_USB_SUSPEND_EVENT:
      usb_ffb::suspended(true);
      break;
    case ARDUINO_USB_RESUME_EVENT:
      usb_ffb::suspended(false);
      break;
    default:
      break;
  }
}

void configure_usb_identity() {
  char serial[13]{};
  const std::uint64_t chip_id = ESP.getEfuseMac();
  std::snprintf(serial, sizeof(serial), "%012" PRIX64,
                chip_id & UINT64_C(0x0000FFFFFFFFFFFF));

  USB.VID(kPrivateTestVid);
  USB.PID(kPrivateTestPid);
  USB.firmwareVersion(0x0600);
  USB.productName("ESP32-S3 Passive Wheel + Digital Pedals (FFB DISABLED)");
  USB.manufacturerName("Aura DIY Racing");
  USB.serialNumber(serial);
  USB.usbPower(100);
}

}  // namespace

void setup() {
  // This must be the first hardware action. External pull-downs on both L298N
  // enable pins are still required while the ESP32-S3 is reset or unpowered.
  force_motor_gpio_safe();
  Serial.begin(115200);

  const auto loaded = g_storage.load();
  const wheel::ConfigRecord boot_config = loaded.valid_from_nvs
      ? loaded.record
      : wheel::safe_default_config();
  usb_ffb::initialize(g_shared, boot_config);
  if (!start_control_task(g_shared, boot_config, loaded.valid_from_nvs)) {
    g_shared.request_fault(wheel::FaultCode::internal_error);
  }

  configure_usb_identity();
  USB.onEvent(usb_event);
  if (!ffb_hid::begin() || !USB.begin()) {
    g_shared.request_fault(wheel::FaultCode::internal_error);
  }
}

void loop() {
  usb_ffb::service();

  // UART0 commissioning trace on the CH343 programming port.
  static std::uint32_t last_input_trace_ms = 0;
  const std::uint32_t now_ms = millis();
  if (now_ms - last_input_trace_ms >= 250u) {
    last_input_trace_ms = now_ms;
    const auto status = g_shared.snapshot();
    Serial.printf("INPUT usb=%u p13=%d p14=%d brake48=%d accel38=%d "
                  "buttons=0x%08" PRIX32 " loop_us=%u\r\n",
                  g_shared.usb_mounted.load(std::memory_order_acquire) ? 1u : 0u,
                  gpio_get_level(board::kShiftUp),
                  gpio_get_level(board::kShiftDown),
                  gpio_get_level(board::kBrakeButton),
                  gpio_get_level(board::kAcceleratorButton), status.buttons,
                  status.loop_time_us);
  }

  wheel::ConfigRecord pending{};
  while (g_shared.pending_configs.pop(pending)) {
    if (!g_storage.save(pending)) {
      g_shared.request_fault(wheel::FaultCode::internal_error);
    }
  }
  while (g_shared.paddle_configs_to_save.pop(pending)) {
    if (!g_storage.save(pending)) {
      g_shared.request_fault(wheel::FaultCode::internal_error);
    }
  }
  delay(1);
}
