#include "motor_output.hpp"

#include <algorithm>

#include "board_config.hpp"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

namespace {
constexpr ledc_mode_t kSpeedMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_0;
constexpr ledc_timer_t kTimer = LEDC_TIMER_0;
constexpr unsigned kDutyBits = 12;
constexpr std::uint32_t kMaximumPwmCount = (1u << kDutyBits) - 1u;
}

bool MotorOutput::initialize() {
  // Establish GPIO-low before connecting the PWM peripheral. External
  // pull-downs on both L298N enable pins remain mandatory.
  gpio_config_t gpio{};
  gpio.pin_bit_mask = (1ULL << board::kSharedPwm) | (1ULL << board::kDirection1) |
                      (1ULL << board::kDirection2);
  gpio.mode = GPIO_MODE_OUTPUT;
  gpio.pull_down_en = GPIO_PULLDOWN_ENABLE;
  gpio.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&gpio) != ESP_OK) return false;
  gpio_set_level(board::kSharedPwm, 0);
  gpio_set_level(board::kDirection1, 0);
  gpio_set_level(board::kDirection2, 0);

  ledc_timer_config_t timer{};
  timer.speed_mode = kSpeedMode;
  timer.duty_resolution = LEDC_TIMER_12_BIT;
  timer.timer_num = kTimer;
  timer.freq_hz = board::kPwmFrequencyHz;
  timer.clk_cfg = LEDC_AUTO_CLK;
  if (ledc_timer_config(&timer) != ESP_OK) return false;

  ledc_channel_config_t channel{};
  channel.gpio_num = board::kSharedPwm;
  channel.speed_mode = kSpeedMode;
  channel.channel = kChannel;
  channel.timer_sel = kTimer;
  channel.duty = 0;
  channel.hpoint = 0;
  initialized_ = ledc_channel_config(&channel) == ESP_OK;
  disable();
  return initialized_;
}

void MotorOutput::set_force(std::int16_t command, float global_gain, float maximum_force,
                            float maximum_duty, float slew_per_tick) {
  if (!initialized_) return;
  apply(control_.update(command, global_gain, maximum_force, maximum_duty, slew_per_tick));
}

void MotorOutput::disable() {
  control_.disable();
  if (initialized_) {
    ledc_set_duty(kSpeedMode, kChannel, 0);
    ledc_update_duty(kSpeedMode, kChannel);
  }
  gpio_set_level(board::kSharedPwm, 0);
  gpio_set_level(board::kDirection1, 0);
  gpio_set_level(board::kDirection2, 0);
}

void MotorOutput::apply(const wheel::MotorStep& step) {
  if (!step.enabled) {
    ledc_set_duty(kSpeedMode, kChannel, 0);
    ledc_update_duty(kSpeedMode, kChannel);
    if (step.direction_changed) {
      gpio_set_level(board::kDirection1, step.direction > 0);
      gpio_set_level(board::kDirection2, step.direction < 0);
    }
    return;
  }
  gpio_set_level(board::kDirection1, step.direction > 0);
  gpio_set_level(board::kDirection2, step.direction < 0);
  const auto duty = static_cast<std::uint32_t>(
      std::clamp(step.duty, 0.0F, 1.0F) * static_cast<float>(kMaximumPwmCount));
  ledc_set_duty(kSpeedMode, kChannel, duty);
  ledc_update_duty(kSpeedMode, kChannel);
}

