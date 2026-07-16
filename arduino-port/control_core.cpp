#include "control_core.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "as5600.hpp"
#include "board_config.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor_output.hpp"
#include "src/wheel/axis_encoder.hpp"
#include "src/wheel/effect_engine.hpp"
#include "src/wheel/safety.hpp"
#include "wheel_controls.hpp"

namespace {

struct Context {
  RuntimeShared* shared{nullptr};
  wheel::ConfigRecord boot_config{};
  bool boot_config_from_nvs{false};
  TaskHandle_t task{nullptr};
};
Context g_context;

void apply_command(const wheel::ControlCommand& command, wheel::EffectEngine& engine,
                   wheel::ConfigRecord& config, wheel::AxisEncoderMath& encoder,
                   wheel::SafetySupervisor& safety, bool& host_enabled,
                   bool& clear_requested, bool& configuration_changed,
                   std::uint32_t& acknowledged_disable_epoch) {
  switch (command.type) {
    case wheel::ControlCommandType::set_effect:
      engine.update_definition(command.index, command.effect);
      break;
    case wheel::ControlCommandType::free_effect: engine.free(command.index); break;
    case wheel::ControlCommandType::start_effect:
      engine.start(command.index, command.loops, (command.flags & 1u) != 0u,
                   static_cast<std::uint32_t>(esp_timer_get_time() / 1000));
      break;
    case wheel::ControlCommandType::stop_effect: engine.stop(command.index); break;
    case wheel::ControlCommandType::stop_all: engine.stop_all(); break;
    case wheel::ControlCommandType::reset:
      engine.reset();
      host_enabled = false;
      safety.host_disable();
      break;
    case wheel::ControlCommandType::enable_actuators:
      host_enabled = true;
      engine.set_actuators_enabled(true);
      acknowledged_disable_epoch = command.disable_epoch;
      break;
    case wheel::ControlCommandType::disable_actuators:
      host_enabled = false;
      engine.set_actuators_enabled(false);
      safety.host_disable();
      break;
    case wheel::ControlCommandType::pause: engine.set_paused(true); break;
    case wheel::ControlCommandType::resume: engine.set_paused(false); break;
    case wheel::ControlCommandType::set_device_gain: engine.set_device_gain(command.value); break;
    case wheel::ControlCommandType::replace_config:
      if (wheel::config_valid(command.config)) {
        config = command.config;
        encoder.reset();
        host_enabled = false;
        engine.set_actuators_enabled(false);
        safety.host_disable();
        configuration_changed = true;
      }
      break;
    case wheel::ControlCommandType::clear_fault: clear_requested = true; break;
  }
}

void control_task(void* argument) {
  auto& context = *static_cast<Context*>(argument);
  auto& shared = *context.shared;
  wheel::ConfigRecord config = context.boot_config;
  wheel::EffectEngine engine;
  wheel::AxisEncoderMath encoder_math;
  wheel::SafetySupervisor safety;
  AxisEncoder encoder;
  MotorOutput motor;
  WheelControls controls;

  const bool encoder_ready = encoder.initialize();
  const bool motor_ready = board::kPassiveWheelMode || motor.initialize();
  const bool controls_ready = controls.initialize();
  const bool hardware_ready = encoder_ready && motor_ready && controls_ready;
  motor.disable();
  safety.boot_complete();
  bool auto_center_on_first_sample = false;
  if (!context.boot_config_from_nvs || !wheel::config_valid(config)) {
    config = wheel::safe_default_config();
    if constexpr (board::kPassiveWheelMode) {
      auto_center_on_first_sample = true;
    } else {
      safety.latch_fault(wheel::FaultCode::invalid_configuration);
    }
  }
  if (!hardware_ready) safety.latch_fault(wheel::FaultCode::internal_error);
  shared.publish_configuration(config);

  bool host_enabled = false;
  bool clear_requested = false;
  std::uint32_t acknowledged_disable_epoch = 0;
  std::uint8_t io_failures = 0;
  std::uint8_t overruns = 0;
  std::uint16_t previous_raw = 0;
  bool previous_raw_valid = false;
  std::uint16_t center_hold_ticks = 0;
  bool center_gesture_latched = false;
  wheel::AxisState axis{};
  wheel::TelemetrySnapshot telemetry{};
  TickType_t last_wake = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1));
    const std::int64_t started_us = esp_timer_get_time();

    if (shared.disable_epoch.load(std::memory_order_acquire) != acknowledged_disable_epoch)
      motor.disable();
    if (shared.command_overflow.exchange(false, std::memory_order_acq_rel)) {
      safety.latch_fault(wheel::FaultCode::usb_command_overflow);
    }
    const auto asynchronous_fault =
        shared.asynchronous_fault.exchange(wheel::FaultCode::none, std::memory_order_acq_rel);
    if (asynchronous_fault != wheel::FaultCode::none) safety.latch_fault(asynchronous_fault);

    wheel::ControlCommand command{};
    bool configuration_changed = false;
    while (shared.commands.pop(command)) {
      apply_command(command, engine, config, encoder_math, safety, host_enabled, clear_requested,
                    configuration_changed, acknowledged_disable_epoch);
    }
    if (configuration_changed) shared.publish_configuration(config);
    if (shared.disable_epoch.load(std::memory_order_acquire) != acknowledged_disable_epoch) {
      host_enabled = false;
      engine.set_actuators_enabled(false);
      safety.host_disable();
    }
    if constexpr (board::kPassiveWheelMode) {
      host_enabled = false;
      engine.set_actuators_enabled(false);
      safety.host_disable();
      motor.disable();
    }

    AxisEncoder::Sample sample = encoder.read();
    if (!sample.io_ok) sample = encoder.read();  // One bounded retry.
    if (!sample.io_ok) {
      if (io_failures < UINT8_MAX) ++io_failures;
      if (io_failures >= board::kConsecutiveIoFailures)
        safety.latch_fault(wheel::FaultCode::encoder_failure);
    } else {
      io_failures = 0;
      if (!sample.magnet_detected()) {
        safety.latch_fault(wheel::FaultCode::magnet_missing);
      }
      if (sample.magnet_detected() && previous_raw_valid &&
          std::abs(wheel::AxisEncoderMath::wrapped_delta(sample.raw_angle, previous_raw)) >
              board::kImplausibleDeltaCounts) {
        safety.latch_fault(wheel::FaultCode::encoder_implausible_motion);
      }
      if (sample.magnet_detected()) {
        if (auto_center_on_first_sample) {
          config.runtime.encoder.zero_offset = sample.raw_angle;
          wheel::config_finalize(config);
          shared.publish_configuration(config);
          auto_center_on_first_sample = false;
        }
        if (!previous_raw_valid) encoder_math.reset();
        previous_raw = sample.raw_angle;
        previous_raw_valid = true;
        axis = encoder_math.update(sample.raw_angle, 0.001F, config.runtime.encoder);
      } else {
        previous_raw_valid = false;
        axis = {};
      }
    }

    const std::uint32_t buttons = controls.sample();
    constexpr std::uint32_t kBothPaddles = (1u << 7u) | (1u << 6u);
    constexpr std::uint16_t kCenterHoldTicks = 2000u;
    if ((buttons & kBothPaddles) == kBothPaddles) {
      if (!center_gesture_latched && sample.reference_valid()) {
        if (center_hold_ticks < kCenterHoldTicks) ++center_hold_ticks;
        if (center_hold_ticks == kCenterHoldTicks) {
          config.runtime.encoder.zero_offset = sample.raw_angle;
          wheel::config_finalize(config);
          encoder_math.reset();
          axis = encoder_math.update(sample.raw_angle, 0.001F, config.runtime.encoder);
          previous_raw = sample.raw_angle;
          previous_raw_valid = true;
          auto_center_on_first_sample = false;
          shared.publish_configuration(config);
          if (!shared.paddle_configs_to_save.push(config)) {
            shared.request_fault(wheel::FaultCode::internal_error);
          }
          center_gesture_latched = true;
        }
      } else if (!sample.reference_valid()) {
        center_hold_ticks = 0;
      }
    } else {
      center_hold_ticks = 0;
      center_gesture_latched = false;
    }

    const bool configuration_ready = wheel::config_valid(config) &&
        config.runtime.maximum_force > 0.0F && config.runtime.maximum_duty > 0.0F;
    wheel::SafetyInputs safety_inputs{
        sample.io_ok && io_failures == 0,
        sample.reference_valid(),
        shared.usb_mounted.load(std::memory_order_acquire),
        shared.usb_suspended.load(std::memory_order_acquire),
        host_enabled,
        configuration_ready,
        std::abs(encoder_math.velocity_counts_per_second()) <
            board::kArmingVelocityCountsPerSecond};
    if (clear_requested) {
      safety.clear_fault(safety_inputs, true);
      clear_requested = false;
    }
    safety.update(safety_inputs);

    std::int16_t force = 0;
    if constexpr (board::kPassiveWheelMode) {
      motor.disable();
    } else {
      force = engine.tick(axis, static_cast<std::uint32_t>(esp_timer_get_time() / 1000));
      if (!safety.motor_allowed() ||
          shared.disable_epoch.load(std::memory_order_acquire) != acknowledged_disable_epoch) {
        force = 0;
        motor.disable();
      } else {
        motor.set_force(force, config.runtime.global_gain, config.runtime.maximum_force,
                        config.runtime.maximum_duty, config.runtime.duty_slew_per_tick);
      }
    }

    telemetry.buttons = buttons;
    telemetry.encoder_position = encoder_math.position_counts();
    telemetry.encoder_velocity = encoder_math.velocity_counts_per_second();
    telemetry.hid_position = static_cast<std::int16_t>(axis.position);
    telemetry.force_command = force;
    telemetry.raw_angle = sample.raw_angle;
    ++telemetry.tick_count;
    telemetry.first_fault = safety.first_fault();
    telemetry.state = safety.state();
    telemetry.encoder_io_errors = sample.io_error_count;
    telemetry.magnet_status = sample.status;
    telemetry.encoder_io_ok = sample.io_ok ? 1u : 0u;
    telemetry.effects_playing = engine.any_playing() ? 1u : 0u;
    telemetry.paused = engine.paused() ? 1u : 0u;
    shared.publish(telemetry);

    const std::int64_t elapsed_us = esp_timer_get_time() - started_us;
    if (elapsed_us > board::kHardOverrunUs) {
      if (overruns < UINT8_MAX) ++overruns;
      if (overruns >= board::kRepeatedOverruns) {
        safety.latch_fault(wheel::FaultCode::control_overrun);
        motor.disable();
        force = 0;
      }
    } else {
      overruns = 0;
    }
    telemetry.loop_time_us = static_cast<std::uint16_t>(std::min<std::int64_t>(elapsed_us, 65535));
  }
}

}  // namespace

bool start_control_task(RuntimeShared& shared, const wheel::ConfigRecord& boot_config,
                        bool boot_config_from_nvs) {
  if (g_context.task != nullptr) return false;
  g_context.shared = &shared;
  g_context.boot_config = boot_config;
  g_context.boot_config_from_nvs = boot_config_from_nvs;
  return xTaskCreatePinnedToCore(control_task, "wheel-control", 8192, &g_context,
                                 configMAX_PRIORITIES - 2, &g_context.task, 1) == pdPASS;
}
