#include "motor_control.hpp"

#include <algorithm>
#include <cmath>

namespace wheel {

void MotorControlMath::disable() { *this = MotorControlMath{}; }

MotorStep MotorControlMath::update(std::int16_t force, float global_gain,
                                   float maximum_force, float maximum_duty,
                                   float slew_per_tick) {
  const float normalized = static_cast<float>(force) / 32767.0F;
  const float limited_force = std::clamp(normalized * std::clamp(global_gain, 0.0F, 1.0F),
                                         -std::clamp(maximum_force, 0.0F, 1.0F),
                                         std::clamp(maximum_force, 0.0F, 1.0F));
  const float target_duty = std::min(std::abs(limited_force),
                                     std::clamp(maximum_duty, 0.0F, 1.0F));
  const std::int8_t requested_direction = limited_force > 0.0F ? 1 :
                                          (limited_force < 0.0F ? -1 : 0);
  const float slew = std::clamp(slew_per_tick, 0.0F, 1.0F);

  if (phase_ == ReversalPhase::disabled_period_one) {
    phase_ = ReversalPhase::disabled_period_two;
    return {};
  }
  if (phase_ == ReversalPhase::disabled_period_two) {
    direction_ = pending_direction_;
    phase_ = ReversalPhase::driving;
    return MotorStep{direction_, 0.0F, false, true};
  }

  if (requested_direction != 0 && direction_ != 0 && requested_direction != direction_) {
    applied_duty_ = 0.0F;
    pending_direction_ = requested_direction;
    phase_ = ReversalPhase::disabled_period_one;
    return {};
  }

  if (requested_direction == 0 || target_duty == 0.0F) {
    applied_duty_ = 0.0F;
    return MotorStep{direction_, 0.0F, false, false};
  }
  if (direction_ == 0) {
    direction_ = requested_direction;
  }
  applied_duty_ += std::clamp(target_duty - applied_duty_, -slew, slew);
  return MotorStep{direction_, applied_duty_, applied_duty_ > 0.0F, false};
}

}  // namespace wheel
