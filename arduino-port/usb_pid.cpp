#include "usb_pid.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "esp_timer.h"
#include "ffb_hid_device.hpp"
#include "usb_pid_compat.hpp"
#include "src/wheel/effect_engine.hpp"
#include "src/wheel/usb_pid_reports.hpp"

namespace usb_ffb {
namespace {

#pragma pack(push, 1)
struct WheelInputReport {
  std::uint32_t buttons;
  std::int16_t steering;
  std::uint8_t hat;
};
struct EnvelopeReport { std::uint8_t index; std::uint16_t attack; std::uint16_t fade;
  std::uint32_t attack_ms; std::uint32_t fade_ms; };
struct ConditionReport { std::uint8_t index; std::uint8_t offset; std::int16_t center;
  std::int16_t positive_coefficient; std::int16_t negative_coefficient;
  std::uint16_t positive_saturation; std::uint16_t negative_saturation;
  std::uint16_t dead_band; };
struct PeriodicReport { std::uint8_t index; std::uint16_t magnitude; std::int16_t offset;
  std::uint16_t phase; std::uint32_t period_ms; };
struct ConstantReport { std::uint8_t index; std::int16_t magnitude; };
struct RampReport { std::uint8_t index; std::int16_t start; std::int16_t end; };
struct OperationReport { std::uint8_t index; std::uint8_t operation; std::uint8_t loops; };
struct BlockLoadReport { std::uint8_t index; std::uint8_t status; std::uint16_t available; };
struct PoolReport { std::uint16_t pool_size; std::uint8_t simultaneous; std::uint8_t flags; };
struct StatusReport {
  std::uint8_t version;
  std::uint8_t length;
  std::int32_t encoder_position;
  float encoder_velocity;
  std::int16_t hid_position;
  std::int16_t force_command;
  std::uint16_t raw_angle;
  std::uint16_t loop_time_us;
  std::uint32_t tick_count;
  std::uint32_t buttons;
  std::uint32_t encoder_io_errors;
  std::uint16_t first_fault;
  std::uint8_t state;
  std::uint8_t encoder_io_ok;
  std::uint8_t magnet_status;
  std::uint8_t effects_playing;
  std::uint8_t paused;
};
#pragma pack(pop)

static_assert(sizeof(WheelInputReport) == 7);
static_assert(sizeof(EnvelopeReport) == 13);
static_assert(sizeof(ConditionReport) == 14);
static_assert(sizeof(PeriodicReport) == 11);
static_assert(sizeof(StatusReport) == 37);
static_assert(sizeof(wheel::ConfigRecord) == 40);
static_assert(sizeof(wheel::ConfigRecord) + 1 <= 63);

RuntimeShared* g_shared = nullptr;
std::array<wheel::Effect, wheel::kEffectSlotCount> g_effects{};
BlockLoadReport g_block_load{0, 3, static_cast<std::uint16_t>(wheel::kEffectSlotCount)};
std::uint32_t g_last_wheel_ms = 0;
std::uint32_t g_last_state_ms = 0;
std::uint8_t g_last_state = 0xff;

template <typename T>
bool decode_exact(const std::uint8_t* data, std::uint16_t size, T& report) {
  if (data == nullptr || size != sizeof(T)) return false;
  std::memcpy(&report, data, sizeof(T));
  return true;
}

bool zero_padding(const std::uint8_t* data, std::size_t begin, std::size_t size) {
  for (std::size_t i = begin; i < size; ++i) if (data[i] != 0) return false;
  return true;
}

wheel::EffectType effect_type(std::uint8_t type) {
  switch (type) {
    case 1: return wheel::EffectType::constant;
    case 2: return wheel::EffectType::ramp;
    case 3: return wheel::EffectType::square;
    case 4: return wheel::EffectType::sine;
    case 5: return wheel::EffectType::triangle;
    case 6: return wheel::EffectType::sawtooth_up;
    case 7: return wheel::EffectType::sawtooth_down;
    case 8: return wheel::EffectType::spring;
    case 9: return wheel::EffectType::damper;
    case 10: return wheel::EffectType::friction;
    case 11: return wheel::EffectType::inertia;
    default: return wheel::EffectType::none;
  }
}

bool periodic(wheel::EffectType type) {
  return type == wheel::EffectType::square || type == wheel::EffectType::sine ||
         type == wheel::EffectType::triangle || type == wheel::EffectType::sawtooth_up ||
         type == wheel::EffectType::sawtooth_down;
}

bool conditional(wheel::EffectType type) {
  return type == wheel::EffectType::spring || type == wheel::EffectType::damper ||
         type == wheel::EffectType::friction || type == wheel::EffectType::inertia;
}

wheel::Effect* effect(std::uint8_t index) {
  if (index == 0 || index > g_effects.size()) return nullptr;
  return &g_effects[index - 1];
}

bool enqueue(wheel::ControlCommand command) {
  return g_shared != nullptr && g_shared->push_command(command);
}

bool enqueue_simple(wheel::ControlCommandType type, std::uint8_t index = 0,
                    std::uint8_t loops = 1, std::uint8_t flags = 0) {
  wheel::ControlCommand command{};
  command.type = type;
  command.index = index;
  command.loops = loops;
  command.flags = flags;
  if (type == wheel::ControlCommandType::enable_actuators && g_shared != nullptr)
    command.disable_epoch = g_shared->disable_epoch.load(std::memory_order_acquire);
  return enqueue(command);
}

std::uint16_t free_effect_slots() {
  return static_cast<std::uint16_t>(std::count_if(
      g_effects.begin(), g_effects.end(), [](const wheel::Effect& item) {
        return !item.allocated;
      }));
}

bool queue_config_update(const wheel::ConfigRecord& candidate) {
  if (g_shared == nullptr) return false;
  g_shared->request_disable();
  if (!g_shared->pending_configs.push(candidate)) {
    g_shared->command_overflow.store(true, std::memory_order_release);
    return false;
  }
  wheel::ControlCommand command{};
  command.type = wheel::ControlCommandType::replace_config;
  command.config = candidate;
  if (!enqueue(command)) return false;
  return true;
}

bool publish(std::uint8_t index) {
  const auto* source = effect(index);
  if (source == nullptr || !source->allocated) return false;
  wheel::ControlCommand command{};
  command.type = wheel::ControlCommandType::set_effect;
  command.index = index;
  command.effect = *source;
  return enqueue(command);
}

void create_effect(const std::uint8_t* data, std::uint16_t size) {
  // HID New Effect payload is type + 10-bit byte count + six padding bits.
  if (data == nullptr || size != 3 || (data[2] & 0xfcU) != 0) return;
  const std::uint16_t requested_bytes =
      static_cast<std::uint16_t>(data[1] | ((data[2] & 0x03u) << 8u));
  if (requested_bytes > 511u) return;
  const auto type = effect_type(data[0]);
  g_block_load = {0, 3, 0};
  if (type == wheel::EffectType::none) return;
  for (std::size_t i = 0; i < g_effects.size(); ++i) {
    if (!g_effects[i].allocated) {
      g_effects[i] = wheel::Effect{};
      g_effects[i].allocated = true;
      g_effects[i].type = type;
      g_block_load = {static_cast<std::uint8_t>(i + 1), 1, free_effect_slots()};
      if (!publish(g_block_load.index)) {
        g_effects[i] = wheel::Effect{};
        g_block_load = {0, 2, free_effect_slots()};
      }
      return;
    }
  }
  g_block_load.status = 2;
}

void parse_output(std::uint8_t id, const std::uint8_t* data, std::uint16_t size) {
  switch (id) {
    case HID_ID_EFFREP: {
      const auto report = wheel::usb_pid_reports::decode_set_effect(data, size);
      if (!report || report->index == 0 || report->index > g_effects.size() ||
          report->effect_type < 1 || report->effect_type > 11 ||
          report->trigger_button > 8 || report->type_specific_block_offset != 0) return;
      auto* destination = effect(report->index);
      const auto type = effect_type(report->effect_type);
      if (!destination->allocated || destination->type != type) return;
      destination->duration_ms = report->duration_ms;
      destination->start_delay_ms = report->start_delay_ms;
      destination->sample_period_ms = std::max<std::uint16_t>(report->sample_period_ms, 1);
      destination->trigger_repeat_ms = report->trigger_repeat_ms;
      destination->gain = static_cast<std::uint16_t>(report->gain * 32767u / 255u);
      destination->trigger_button = report->trigger_button;
      destination->direction = wheel::usb_pid_reports::direction_x_q15(*report);
      publish(report->index);
      return;
    }
    case HID_ID_ENVREP: {
      EnvelopeReport report{};
      if (!decode_exact(data, size, report) || report.attack > 32767 || report.fade > 32767 ||
          report.attack_ms > 32767u || report.fade_ms > 32767u) return;
      auto* destination = effect(report.index);
      if (!destination || !destination->allocated) return;
      destination->envelope = {report.attack, report.fade,
          static_cast<std::uint16_t>(report.attack_ms), static_cast<std::uint16_t>(report.fade_ms)};
      publish(report.index);
      return;
    }
    case HID_ID_CONDREP: {
      ConditionReport report{};
      if (!decode_exact(data, size, report) || report.offset != 0 ||
          report.positive_saturation > 32767 || report.negative_saturation > 32767 ||
          report.dead_band > 32767) return;
      auto* destination = effect(report.index);
      if (!destination || !destination->allocated || !conditional(destination->type)) return;
      destination->condition = {report.center, report.positive_coefficient,
          report.negative_coefficient, report.positive_saturation,
          report.negative_saturation, report.dead_band};
      publish(report.index);
      return;
    }
    case HID_ID_PRIDREP: {
      PeriodicReport report{};
      if (!decode_exact(data, size, report) || report.magnitude > 32767 ||
          report.phase >= 36000 || report.period_ms == 0 || report.period_ms > 32767u) return;
      auto* destination = effect(report.index);
      if (!destination || !destination->allocated || !periodic(destination->type)) return;
      destination->magnitude = static_cast<std::int16_t>(report.magnitude);
      destination->offset = report.offset;
      destination->phase_hundredth_deg = report.phase;
      destination->period_ms = static_cast<std::uint16_t>(report.period_ms);
      publish(report.index);
      return;
    }
    case HID_ID_CONSTREP: {
      ConstantReport report{};
      if (!decode_exact(data, size, report)) return;
      auto* destination = effect(report.index);
      if (!destination || !destination->allocated ||
          destination->type != wheel::EffectType::constant) return;
      destination->magnitude = report.magnitude;
      publish(report.index);
      return;
    }
    case HID_ID_RAMPREP: {
      RampReport report{};
      if (!decode_exact(data, size, report)) return;
      auto* destination = effect(report.index);
      if (!destination || !destination->allocated || destination->type != wheel::EffectType::ramp)
        return;
      destination->ramp_start = report.start;
      destination->ramp_end = report.end;
      publish(report.index);
      return;
    }
    case HID_ID_EFOPREP: {
      OperationReport report{};
      if (!decode_exact(data, size, report) || !effect(report.index) ||
          !effect(report.index)->allocated || report.operation < 1 || report.operation > 3) return;
      if (report.operation == 3) enqueue_simple(wheel::ControlCommandType::stop_effect, report.index);
      else enqueue_simple(wheel::ControlCommandType::start_effect, report.index, report.loops,
                          report.operation == 2 ? 1 : 0);
      return;
    }
    case HID_ID_BLKFRREP:
      if (data && size == 1 && effect(data[0]) && effect(data[0])->allocated) {
        if (enqueue_simple(wheel::ControlCommandType::free_effect, data[0]))
          g_effects[data[0] - 1] = wheel::Effect{};
      }
      return;
    case HID_ID_CTRLREP: {
      const auto control = wheel::usb_pid_reports::decode_device_control(data, size);
      if (!control) return;
      using DeviceControl = wheel::usb_pid_reports::DeviceControl;
      if (*control == DeviceControl::enable_actuators)
        enqueue_simple(wheel::ControlCommandType::enable_actuators);
      else if (*control == DeviceControl::disable_actuators) {
        g_shared->request_disable();
        enqueue_simple(wheel::ControlCommandType::disable_actuators);
      } else if (*control == DeviceControl::stop_all_effects)
        enqueue_simple(wheel::ControlCommandType::stop_all);
      else if (*control == DeviceControl::device_reset) {
        g_shared->request_disable();
        if (enqueue_simple(wheel::ControlCommandType::reset)) g_effects.fill(wheel::Effect{});
      } else if (*control == DeviceControl::device_pause)
        enqueue_simple(wheel::ControlCommandType::pause);
      else if (*control == DeviceControl::device_continue)
        enqueue_simple(wheel::ControlCommandType::resume);
      return;
    }
    case HID_ID_GAINREP:
      if (data && size == 1) {
        wheel::ControlCommand command{};
        command.type = wheel::ControlCommandType::set_device_gain;
        command.value = static_cast<std::uint16_t>(data[0] * 32767u / 255u);
        enqueue(command);
      }
      return;
    default: return;
  }
}

void parse_vendor(const std::uint8_t* data, std::uint16_t size) {
  if (!data || size != 63) return;
  if (data[0] == 1 && zero_padding(data, 1 + sizeof(wheel::ConfigRecord), size)) {
    wheel::ConfigRecord candidate{};
    std::memcpy(&candidate, data + 1, sizeof(candidate));
    if (!wheel::config_valid(candidate)) return;
    queue_config_update(candidate);
  } else if (data[0] == 2 && zero_padding(data, 1, size)) {
    enqueue_simple(wheel::ControlCommandType::clear_fault);
  } else if (data[0] == 3 && zero_padding(data, 1, size)) {
    const auto status = g_shared->snapshot();
    if (status.encoder_io_ok == 0 || (status.magnet_status & 0x20u) == 0u) return;
    wheel::ConfigRecord candidate = g_shared->configuration_snapshot();
    candidate.runtime.encoder.zero_offset = status.raw_angle;
    wheel::config_finalize(candidate);
    queue_config_update(candidate);
  }
}

template <typename T>
std::uint16_t copy_report(const T& report, std::uint8_t* data, std::uint16_t capacity,
                          bool pad = false) {
  const std::size_t needed = pad ? 63 : sizeof(T);
  if (!data || capacity < needed) return 0;
  if (pad) std::memset(data, 0, needed);
  std::memcpy(data, &report, sizeof(T));
  return static_cast<std::uint16_t>(needed);
}

}  // namespace

void initialize(RuntimeShared& shared, const wheel::ConfigRecord& config) {
  g_shared = &shared;
  shared.publish_configuration(config);
  g_effects.fill(wheel::Effect{});
}

void service() {
  if (!g_shared || !g_shared->usb_mounted.load(std::memory_order_acquire) ||
      g_shared->usb_suspended.load(std::memory_order_acquire) || !ffb_hid::ready()) return;
  const auto now = static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
  const auto status = g_shared->snapshot();
  const std::uint8_t state = static_cast<std::uint8_t>(
      (status.paused ? 0x01u : 0u) |
      (status.state == wheel::SafetyState::armed ? 0x02u : 0u) |
      (status.first_fault == wheel::FaultCode::none ? 0x08u : 0u) |
      (status.effects_playing ? 0x10u : 0u));
  if ((state != g_last_state || now - g_last_state_ms >= 100) && now - g_last_state_ms >= 10) {
    if (ffb_hid::send_report(kStateReportId, &state, sizeof(state))) {
      g_last_state = state;
      g_last_state_ms = now;
    }
  } else if (now != g_last_wheel_ms) {
    const WheelInputReport report{status.buttons, status.hid_position, 0};
    if (ffb_hid::send_report(kWheelReportId, &report, sizeof(report)))
      g_last_wheel_ms = now;
  }
}

void set_report(std::uint8_t report_id, hid_report_type_t report_type,
                const std::uint8_t* data, std::uint16_t size) {
  if (!data) return;
  if (report_id == 0 && size > 0) { report_id = *data++; --size; }
  if (report_type == HID_REPORT_TYPE_FEATURE && report_id == HID_ID_NEWEFREP)
    create_effect(data, size);
  else if (report_type == HID_REPORT_TYPE_FEATURE && report_id == kConfigReportId)
    parse_vendor(data, size);
  else if (report_type == HID_REPORT_TYPE_OUTPUT)
    parse_output(report_id, data, size);
}

std::uint16_t get_report(std::uint8_t report_id, hid_report_type_t,
                         std::uint8_t* data, std::uint16_t capacity) {
  if (report_id == HID_ID_BLKLDREP) return copy_report(g_block_load, data, capacity);
  if (report_id == HID_ID_POOLREP) {
    const PoolReport pool{static_cast<std::uint16_t>(wheel::kEffectSlotCount),
                          static_cast<std::uint8_t>(wheel::kEffectSlotCount), 1};
    return copy_report(pool, data, capacity);
  }
  if (report_id == kConfigReportId && g_shared) {
    const auto config = g_shared->configuration_snapshot();
    return copy_report(config, data, capacity, true);
  }
  if (report_id == kStatusReportId && g_shared) {
    const auto value = g_shared->snapshot();
    const StatusReport report{4, sizeof(StatusReport), value.encoder_position,
        value.encoder_velocity, value.hid_position, value.force_command, value.raw_angle,
        value.loop_time_us, value.tick_count, value.buttons, value.encoder_io_errors,
        static_cast<std::uint16_t>(value.first_fault), static_cast<std::uint8_t>(value.state),
        value.encoder_io_ok, value.magnet_status, value.effects_playing, value.paused};
    return copy_report(report, data, capacity, true);
  }
  return 0;
}

void mounted(bool value) {
  if (!g_shared) return;
  g_shared->usb_mounted.store(value, std::memory_order_release);
  if (!value) {
    g_shared->request_disable();
  }
}

void suspended(bool value) {
  if (!g_shared) return;
  g_shared->usb_suspended.store(value, std::memory_order_release);
  if (value) {
    g_shared->request_disable();
  }
}

}  // namespace usb_ffb
