#pragma once

#include <atomic>
#include <cstdint>

#include "src/wheel/config.hpp"
#include "src/wheel/control_protocol.hpp"
#include "src/wheel/spsc_queue.hpp"

struct RuntimeShared {
  wheel::SpscQueue<wheel::ControlCommand, 65> commands{};
  wheel::SpscQueue<wheel::ConfigRecord, 5> pending_configs{};
  wheel::SpscQueue<wheel::ConfigRecord, 3> paddle_configs_to_save{};
  std::atomic<std::uint32_t> disable_epoch{1};
  std::atomic<bool> usb_mounted{false};
  std::atomic<bool> usb_suspended{false};
  std::atomic<bool> command_overflow{false};
  std::atomic<wheel::FaultCode> asynchronous_fault{wheel::FaultCode::none};
  std::atomic<std::int32_t> encoder_position{0};
  std::atomic<float> encoder_velocity{0.0F};
  std::atomic<std::int16_t> hid_position{0};
  std::atomic<std::int16_t> force_command{0};
  std::atomic<std::uint16_t> raw_angle{0};
  std::atomic<std::uint16_t> loop_time_us{0};
  std::atomic<std::uint32_t> tick_count{0};
  std::atomic<std::uint32_t> buttons{0};
  std::atomic<std::uint32_t> encoder_io_errors{0};
  std::atomic<wheel::FaultCode> first_fault{wheel::FaultCode::none};
  std::atomic<wheel::SafetyState> safety_state{wheel::SafetyState::boot};
  std::atomic<std::uint8_t> magnet_status{0};
  std::atomic<std::uint8_t> encoder_io_ok{0};
  std::atomic<std::uint8_t> effects_playing{0};
  std::atomic<std::uint8_t> paused{0};
  std::atomic<std::uint32_t> telemetry_sequence{0};
  std::atomic<std::uint32_t> config_sequence{0};
  std::atomic<std::int8_t> config_direction{1};
  std::atomic<std::int32_t> config_zero_offset{0};
  std::atomic<float> config_velocity_alpha{0.20F};
  std::atomic<float> config_global_gain{1.0F};
  std::atomic<float> config_maximum_force{0.0F};
  std::atomic<float> config_maximum_duty{0.0F};
  std::atomic<float> config_duty_slew_per_tick{0.002F};

  void publish_configuration(const wheel::ConfigRecord& config) {
    config_sequence.fetch_add(1, std::memory_order_acq_rel);  // Odd: write in progress.
    config_direction.store(config.runtime.encoder.direction, std::memory_order_relaxed);
    config_zero_offset.store(config.runtime.encoder.zero_offset, std::memory_order_relaxed);
    config_velocity_alpha.store(config.runtime.encoder.velocity_alpha, std::memory_order_relaxed);
    config_global_gain.store(config.runtime.global_gain, std::memory_order_relaxed);
    config_maximum_force.store(config.runtime.maximum_force, std::memory_order_relaxed);
    config_maximum_duty.store(config.runtime.maximum_duty, std::memory_order_relaxed);
    config_duty_slew_per_tick.store(config.runtime.duty_slew_per_tick,
                                    std::memory_order_relaxed);
    config_sequence.fetch_add(1, std::memory_order_release);  // Even: complete snapshot.
  }

  wheel::ConfigRecord configuration_snapshot() const {
    for (;;) {
      const auto before = config_sequence.load(std::memory_order_acquire);
      if ((before & 1u) != 0u) continue;
      wheel::ConfigRecord value{};
      value.runtime.encoder.direction = config_direction.load(std::memory_order_relaxed);
      value.runtime.encoder.zero_offset = config_zero_offset.load(std::memory_order_relaxed);
      value.runtime.encoder.velocity_alpha =
          config_velocity_alpha.load(std::memory_order_relaxed);
      value.runtime.global_gain = config_global_gain.load(std::memory_order_relaxed);
      value.runtime.maximum_force = config_maximum_force.load(std::memory_order_relaxed);
      value.runtime.maximum_duty = config_maximum_duty.load(std::memory_order_relaxed);
      value.runtime.duty_slew_per_tick =
          config_duty_slew_per_tick.load(std::memory_order_relaxed);
      wheel::config_finalize(value);
      if (before == config_sequence.load(std::memory_order_acquire)) return value;
    }
  }

  void request_disable() {
    disable_epoch.fetch_add(1, std::memory_order_acq_rel);
  }

  void request_fault(wheel::FaultCode fault) {
    wheel::FaultCode expected = wheel::FaultCode::none;
    asynchronous_fault.compare_exchange_strong(expected, fault, std::memory_order_acq_rel);
    request_disable();
  }

  bool push_command(const wheel::ControlCommand& command) {
    if (commands.push(command)) return true;
    command_overflow.store(true, std::memory_order_release);
    request_disable();
    return false;
  }

  void publish(const wheel::TelemetrySnapshot& snapshot) {
    telemetry_sequence.fetch_add(1, std::memory_order_acq_rel);  // Odd: write in progress.
    encoder_position.store(snapshot.encoder_position, std::memory_order_relaxed);
    encoder_velocity.store(snapshot.encoder_velocity, std::memory_order_relaxed);
    hid_position.store(snapshot.hid_position, std::memory_order_relaxed);
    force_command.store(snapshot.force_command, std::memory_order_relaxed);
    raw_angle.store(snapshot.raw_angle, std::memory_order_relaxed);
    loop_time_us.store(snapshot.loop_time_us, std::memory_order_relaxed);
    tick_count.store(snapshot.tick_count, std::memory_order_relaxed);
    buttons.store(snapshot.buttons, std::memory_order_relaxed);
    encoder_io_errors.store(snapshot.encoder_io_errors, std::memory_order_relaxed);
    first_fault.store(snapshot.first_fault, std::memory_order_relaxed);
    safety_state.store(snapshot.state, std::memory_order_release);
    magnet_status.store(snapshot.magnet_status, std::memory_order_relaxed);
    encoder_io_ok.store(snapshot.encoder_io_ok, std::memory_order_relaxed);
    effects_playing.store(snapshot.effects_playing, std::memory_order_relaxed);
    paused.store(snapshot.paused, std::memory_order_relaxed);
    telemetry_sequence.fetch_add(1, std::memory_order_release);  // Even: complete snapshot.
  }

  wheel::TelemetrySnapshot snapshot() const {
    for (;;) {
      const auto before = telemetry_sequence.load(std::memory_order_acquire);
      if ((before & 1u) != 0u) continue;
      wheel::TelemetrySnapshot value{};
      value.encoder_position = encoder_position.load(std::memory_order_relaxed);
      value.encoder_velocity = encoder_velocity.load(std::memory_order_relaxed);
      value.hid_position = hid_position.load(std::memory_order_relaxed);
      value.force_command = force_command.load(std::memory_order_relaxed);
      value.raw_angle = raw_angle.load(std::memory_order_relaxed);
      value.loop_time_us = loop_time_us.load(std::memory_order_relaxed);
      value.tick_count = tick_count.load(std::memory_order_relaxed);
      value.buttons = buttons.load(std::memory_order_relaxed);
      value.encoder_io_errors = encoder_io_errors.load(std::memory_order_relaxed);
      value.first_fault = first_fault.load(std::memory_order_relaxed);
      value.state = safety_state.load(std::memory_order_relaxed);
      value.magnet_status = magnet_status.load(std::memory_order_relaxed);
      value.encoder_io_ok = encoder_io_ok.load(std::memory_order_relaxed);
      value.effects_playing = effects_playing.load(std::memory_order_relaxed);
      value.paused = paused.load(std::memory_order_relaxed);
      if (before == telemetry_sequence.load(std::memory_order_acquire)) return value;
    }
  }
};
