#pragma once

#include <cstdint>

namespace wheel {

enum class ReversalPhase : std::uint8_t { driving, disabled_period_one, disabled_period_two };

struct MotorStep {
  std::int8_t direction{0};
  float duty{0.0F};
  bool enabled{false};
  bool direction_changed{false};
};

class MotorControlMath {
 public:
  MotorStep update(std::int16_t force, float global_gain, float maximum_force,
                   float maximum_duty, float slew_per_tick);
  void disable();

  ReversalPhase phase() const { return phase_; }
  float applied_duty() const { return applied_duty_; }

 private:
  std::int8_t direction_{0};
  std::int8_t pending_direction_{0};
  float applied_duty_{0.0F};
  ReversalPhase phase_{ReversalPhase::driving};
};

}  // namespace wheel

