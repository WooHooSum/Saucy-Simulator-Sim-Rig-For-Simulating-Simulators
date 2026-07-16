#pragma once

#include <cstdint>

#include "src/wheel/motor_control.hpp"

class MotorOutput {
 public:
  bool initialize();
  void set_force(std::int16_t command, float global_gain, float maximum_force,
                 float maximum_duty, float slew_per_tick);
  void disable();

 private:
  void apply(const wheel::MotorStep& step);
  wheel::MotorControlMath control_{};
  bool initialized_{false};
};
