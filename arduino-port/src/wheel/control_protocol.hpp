#pragma once

#include <cstdint>
#include <type_traits>

#include "config.hpp"
#include "effect_engine.hpp"

namespace wheel {

enum class ControlCommandType : std::uint8_t {
  set_effect,
  free_effect,
  start_effect,
  stop_effect,
  stop_all,
  reset,
  enable_actuators,
  disable_actuators,
  pause,
  resume,
  set_device_gain,
  replace_config,
  clear_fault,
};

struct ControlCommand {
  ControlCommandType type{ControlCommandType::stop_all};
  std::uint8_t index{0};
  std::uint8_t loops{1};
  std::uint8_t flags{0};
  std::uint16_t value{0};
  std::uint32_t disable_epoch{0};
  Effect effect{};
  ConfigRecord config{};
};

static_assert(std::is_trivially_copyable_v<ControlCommand>);

enum class SafetyState : std::uint8_t { boot, disarmed, armed, fault };

enum class FaultCode : std::uint16_t {
  none,
  encoder_failure,
  magnet_missing,
  encoder_implausible_motion,
  control_overrun,
  usb_command_overflow,
  invalid_configuration,
  internal_error,
};

struct TelemetrySnapshot {
  std::int32_t encoder_position{0};
  float encoder_velocity{0.0F};
  std::int16_t hid_position{0};
  std::int16_t force_command{0};
  std::uint16_t raw_angle{0};
  std::uint16_t loop_time_us{0};
  std::uint32_t tick_count{0};
  std::uint32_t buttons{0};
  std::uint32_t encoder_io_errors{0};
  FaultCode first_fault{FaultCode::none};
  SafetyState state{SafetyState::boot};
  std::uint8_t magnet_status{0};
  std::uint8_t encoder_io_ok{0};
  std::uint8_t effects_playing{0};
  std::uint8_t paused{0};
};

}  // namespace wheel
