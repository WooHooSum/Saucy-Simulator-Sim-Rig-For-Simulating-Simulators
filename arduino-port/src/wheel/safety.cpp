#include "safety.hpp"

namespace wheel {

bool SafetySupervisor::healthy_to_arm(const SafetyInputs& in) {
  return in.encoder_valid && in.encoder_ready && in.usb_mounted &&
         !in.usb_suspended && in.host_actuators_enabled &&
         in.configuration_valid && in.motion_below_arming_threshold;
}

void SafetySupervisor::boot_complete() {
  if (state_ == SafetyState::boot) {
    state_ = SafetyState::disarmed;
  }
}

void SafetySupervisor::update(const SafetyInputs& in) {
  if (state_ == SafetyState::fault || state_ == SafetyState::boot) {
    return;
  }
  if (state_ == SafetyState::armed &&
      (!in.encoder_valid || !in.encoder_ready || !in.usb_mounted ||
       in.usb_suspended || !in.host_actuators_enabled ||
       !in.configuration_valid)) {
    state_ = SafetyState::disarmed;
    return;
  }
  if (state_ == SafetyState::disarmed && healthy_to_arm(in)) {
    state_ = SafetyState::armed;
  }
}

void SafetySupervisor::latch_fault(FaultCode code) {
  if (code == FaultCode::none) {
    return;
  }
  if (first_fault_ == FaultCode::none) {
    first_fault_ = code;
  }
  state_ = SafetyState::fault;
}

bool SafetySupervisor::clear_fault(const SafetyInputs& in, bool explicit_request) {
  if (!explicit_request || state_ != SafetyState::fault ||
      !in.encoder_valid || !in.encoder_ready || !in.configuration_valid ||
      in.host_actuators_enabled) {
    return false;
  }
  first_fault_ = FaultCode::none;
  state_ = SafetyState::disarmed;
  return true;
}

void SafetySupervisor::host_disable() {
  if (state_ == SafetyState::armed) {
    state_ = SafetyState::disarmed;
  }
}

}  // namespace wheel
