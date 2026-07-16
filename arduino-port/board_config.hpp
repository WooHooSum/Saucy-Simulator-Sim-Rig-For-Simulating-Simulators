#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"

namespace board {

// Generic 44-pin ESP32-S3-WROOM-1 N16R8 board (2025 V1.4).
inline constexpr gpio_num_t kSharedPwm = GPIO_NUM_42;
inline constexpr gpio_num_t kDirection1 = GPIO_NUM_41;
inline constexpr gpio_num_t kDirection2 = GPIO_NUM_39;
inline constexpr gpio_num_t kI2cSda = GPIO_NUM_8;
inline constexpr gpio_num_t kI2cScl = GPIO_NUM_9;
inline constexpr gpio_num_t kShiftUp = GPIO_NUM_13;
inline constexpr gpio_num_t kShiftDown = GPIO_NUM_14;
inline constexpr gpio_num_t kBrakeButton = GPIO_NUM_48;
inline constexpr gpio_num_t kAcceleratorButton = GPIO_NUM_38;

// Shipping build mode: keep the complete HID/PID interface available while
// physically inhibiting every L298N output. GPIO42, GPIO41, and GPIO39 remain
// low even if the host sends an Enable Actuators or force-effect command.
inline constexpr bool kPassiveWheelMode = true;

inline constexpr std::array<gpio_num_t, 9> kAssignedPins{
    kSharedPwm, kDirection1, kDirection2, kI2cSda,
    kI2cScl, kShiftUp, kShiftDown, kBrakeButton, kAcceleratorButton};

consteval bool assigned_pins_are_unique() {
  for (std::size_t first = 0; first < kAssignedPins.size(); ++first) {
    for (std::size_t second = first + 1; second < kAssignedPins.size(); ++second) {
      if (kAssignedPins[first] == kAssignedPins[second]) return false;
    }
  }
  return true;
}
static_assert(assigned_pins_are_unique(),
              "motor, encoder, and button GPIOs must not overlap");

inline constexpr std::uint8_t kAs5600Address = 0x36;
inline constexpr std::uint32_t kI2cFrequencyHz = 400000;
inline constexpr std::uint32_t kPwmFrequencyHz = 16000;
inline constexpr std::uint32_t kControlPeriodUs = 1000;
inline constexpr std::uint32_t kNormalExecutionUs = 300;
inline constexpr std::uint32_t kHardOverrunUs = 700;
inline constexpr std::uint8_t kConsecutiveIoFailures = 3;
inline constexpr std::uint8_t kRepeatedOverruns = 3;
inline constexpr std::int32_t kImplausibleDeltaCounts = 512;  // 45 degrees/tick.
inline constexpr float kArmingVelocityCountsPerSecond = 40.0F;  // 3.5 deg/s.

}  // namespace board
