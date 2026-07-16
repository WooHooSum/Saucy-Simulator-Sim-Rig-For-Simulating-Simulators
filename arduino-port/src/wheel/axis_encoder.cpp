#include "axis_encoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace wheel {

void AxisEncoderMath::reset() { *this = AxisEncoderMath{}; }

std::int32_t AxisEncoderMath::wrapped_delta(std::uint16_t current,
                                            std::uint16_t previous) {
  std::int32_t delta = static_cast<std::int32_t>(current) - previous;
  if (delta > 2048) {
    delta -= 4096;
  } else if (delta < -2048) {
    delta += 4096;
  }
  return delta;
}

std::int32_t AxisEncoderMath::relative_to_zero(std::uint16_t raw,
                                               std::int32_t zero_offset) {
  const auto zero = static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(zero_offset) & 0x0fffu);
  return wrapped_delta(raw, zero);
}

std::int16_t AxisEncoderMath::counts_to_hid(std::int32_t counts) {
  const auto scaled = static_cast<std::int64_t>(counts) * 32767 / 2048;
  return static_cast<std::int16_t>(std::clamp<std::int64_t>(scaled, -32767, 32767));
}

AxisState AxisEncoderMath::update(std::uint16_t raw_angle, float dt_seconds,
                                  const EncoderConfig& config) {
  const std::int32_t direction = config.direction < 0 ? -1 : 1;
  if (!initialized_) {
    previous_raw_ = raw_angle;
    position_counts_ = direction * relative_to_zero(raw_angle, config.zero_offset);
    previous_position_ = position_counts_;
    initialized_ = true;
  } else {
    position_counts_ += direction * wrapped_delta(raw_angle, previous_raw_);
    previous_raw_ = raw_angle;
  }

  const float dt = std::max(dt_seconds, 0.000001F);
  const float measured_velocity =
      static_cast<float>(position_counts_ - previous_position_) / dt;
  const float alpha = std::clamp(config.velocity_alpha, 0.0001F, 1.0F);
  velocity_ += alpha * (measured_velocity - velocity_);
  const float acceleration = (velocity_ - previous_velocity_) / dt;
  previous_velocity_ = velocity_;
  previous_position_ = position_counts_;

  constexpr float kHidPerCount = 32767.0F / 2048.0F;
  return AxisState{counts_to_hid(position_counts_), velocity_ * kHidPerCount,
                   acceleration * kHidPerCount};
}

}  // namespace wheel
