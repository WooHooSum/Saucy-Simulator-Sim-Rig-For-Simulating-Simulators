#pragma once

#include <cstdint>

#include "config.hpp"
#include "effect_engine.hpp"

namespace wheel {

class AxisEncoderMath {
 public:
  void reset();
  AxisState update(std::uint16_t raw_angle, float dt_seconds,
                   const EncoderConfig& config);

  bool initialized() const { return initialized_; }
  std::int32_t position_counts() const { return position_counts_; }
  float velocity_counts_per_second() const { return velocity_; }

  static std::int32_t wrapped_delta(std::uint16_t current,
                                    std::uint16_t previous);
  static std::int32_t relative_to_zero(std::uint16_t raw,
                                       std::int32_t zero_offset);
  static std::int16_t counts_to_hid(std::int32_t counts);

 private:
  bool initialized_{false};
  std::uint16_t previous_raw_{0};
  std::int32_t position_counts_{0};
  std::int32_t previous_position_{0};
  float velocity_{0.0F};
  float previous_velocity_{0.0F};
};

}  // namespace wheel
