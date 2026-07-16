#include "wheel_controls.hpp"

#include "board_config.hpp"
#include "driver/gpio.h"
#include "esp_err.h"

bool WheelControls::initialize() {
  const std::uint64_t mask =
      (1ULL << board::kShiftUp) | (1ULL << board::kShiftDown) |
      (1ULL << board::kBrakeButton) | (1ULL << board::kAcceleratorButton);
  gpio_config_t input{};
  input.pin_bit_mask = mask;
  input.mode = GPIO_MODE_INPUT;
  input.pull_up_en = GPIO_PULLUP_ENABLE;
  input.pull_down_en = GPIO_PULLDOWN_DISABLE;
  input.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&input) != ESP_OK) return false;

  // Reassert a pull-up-only mode on each input. Paddle released levels are
  // learned at boot so their existing NO/NC wiring keeps working. The pedal
  // switches are explicitly NC-to-ground: LOW is released and HIGH is pressed.
  if (gpio_set_pull_mode(board::kShiftUp, GPIO_PULLUP_ONLY) != ESP_OK ||
      gpio_set_pull_mode(board::kShiftDown, GPIO_PULLUP_ONLY) != ESP_OK ||
      gpio_set_pull_mode(board::kBrakeButton, GPIO_PULLUP_ONLY) != ESP_OK ||
      gpio_set_pull_mode(board::kAcceleratorButton, GPIO_PULLUP_ONLY) != ESP_OK) {
    return false;
  }
  shift_up_idle_level_ = static_cast<std::uint8_t>(gpio_get_level(board::kShiftUp));
  shift_down_idle_level_ = static_cast<std::uint8_t>(gpio_get_level(board::kShiftDown));
  shift_up_integrator_ = 0;
  shift_down_integrator_ = 0;
  brake_integrator_ = 0;
  accelerator_integrator_ = 0;
  stable_buttons_ = 0;
  return true;
}

std::uint32_t WheelControls::sample() {
  const auto update_button = [this](gpio_num_t pin, std::uint8_t bit,
                                    std::uint8_t idle_level, std::uint8_t& count) {
    if (gpio_get_level(pin) != idle_level) {
      if (count < kDebounceTicks) ++count;
      if (count == kDebounceTicks) stable_buttons_ |= 1u << bit;
    } else {
      if (count > 0) --count;
      if (count == 0) stable_buttons_ &= ~(1u << bit);
    }
  };
  // Common PlayStation DirectInput order: L2 is Button 7 (bit 6) and R2 is
  // Button 8 (bit 7). A digital paddle reports fully released or pressed.
  update_button(board::kShiftUp, 7, shift_up_idle_level_,
                shift_up_integrator_);  // GPIO13 -> R2.
  update_button(board::kShiftDown, 6, shift_down_idle_level_,
                shift_down_integrator_);  // GPIO14 -> L2.
  // Common PlayStation DirectInput order: L1 is Button 5 (bit 4) and R1 is
  // Button 6 (bit 5). NC contacts pull LOW while released and open HIGH.
  update_button(board::kBrakeButton, 4, 0, brake_integrator_);  // GPIO48 -> L1.
  update_button(board::kAcceleratorButton, 5, 0,
                accelerator_integrator_);  // GPIO38 -> R1.
  return stable_buttons_;
}
