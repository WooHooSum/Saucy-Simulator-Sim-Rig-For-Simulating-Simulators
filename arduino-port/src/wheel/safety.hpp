#pragma once

#include "control_protocol.hpp"

namespace wheel {

struct SafetyInputs {
  bool encoder_valid{false};
  bool encoder_ready{false};
  bool usb_mounted{false};
  bool usb_suspended{false};
  bool host_actuators_enabled{false};
  bool configuration_valid{false};
  bool motion_below_arming_threshold{false};
};

class SafetySupervisor {
 public:
  void boot_complete();
  void update(const SafetyInputs& inputs);
  void latch_fault(FaultCode code);
  bool clear_fault(const SafetyInputs& inputs, bool explicit_request);
  void host_disable();

  SafetyState state() const { return state_; }
  FaultCode first_fault() const { return first_fault_; }
  bool motor_allowed() const { return state_ == SafetyState::armed; }

 private:
  static bool healthy_to_arm(const SafetyInputs& inputs);
  SafetyState state_{SafetyState::boot};
  FaultCode first_fault_{FaultCode::none};
};

}  // namespace wheel
