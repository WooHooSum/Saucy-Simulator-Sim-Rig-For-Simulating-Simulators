#pragma once

#include <cstdint>

class WheelControls {
 public:
  bool initialize();
  std::uint32_t sample();

 private:
  static constexpr std::uint8_t kDebounceTicks = 5;
  std::uint8_t shift_up_integrator_{0};
  std::uint8_t shift_down_integrator_{0};
  std::uint8_t brake_integrator_{0};
  std::uint8_t accelerator_integrator_{0};
  std::uint8_t shift_up_idle_level_{1};
  std::uint8_t shift_down_idle_level_{1};
  std::uint32_t stable_buttons_{0};
};
