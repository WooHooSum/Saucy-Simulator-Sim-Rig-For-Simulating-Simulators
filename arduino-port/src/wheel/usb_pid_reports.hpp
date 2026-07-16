#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace wheel::usb_pid_reports {

constexpr std::size_t kSetEffectPayloadSize = 17u;
constexpr std::size_t kDeviceControlPayloadSize = 1u;

constexpr std::uint8_t kAxisXEnable = 0x01u;
constexpr std::uint8_t kDirectionEnable = 0x02u;
constexpr std::uint8_t kSetEffectReservedFlags = 0xfcu;

struct SetEffect {
  std::uint8_t index;
  std::uint8_t effect_type;
  std::uint16_t duration_ms;
  std::uint16_t trigger_repeat_ms;
  std::uint16_t sample_period_ms;
  std::uint16_t start_delay_ms;
  std::uint8_t gain;
  std::uint8_t trigger_button;
  std::uint8_t flags;
  std::uint16_t direction_hundredth_deg;
  std::uint16_t type_specific_block_offset;
};

enum class DeviceControl : std::uint8_t {
  enable_actuators = 1,
  disable_actuators = 2,
  stop_all_effects = 4,
  device_reset = 8,
  device_pause = 16,
  device_continue = 32,
};

constexpr std::uint16_t read_u16_le(const std::uint8_t* data) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[0]) |
                                    (static_cast<std::uint16_t>(data[1]) << 8u));
}

inline std::optional<SetEffect> decode_set_effect(const std::uint8_t* data,
                                                  std::size_t size) {
  if (data == nullptr || size != kSetEffectPayloadSize) {
    return std::nullopt;
  }

  SetEffect report{
      data[0],          data[1],
      read_u16_le(data + 2u),  read_u16_le(data + 4u),
      read_u16_le(data + 6u),  read_u16_le(data + 8u),
      data[10],         data[11],
      data[12],         read_u16_le(data + 13u),
      read_u16_le(data + 15u),
  };

  if ((report.flags & kSetEffectReservedFlags) != 0u ||
      report.direction_hundredth_deg >= 36000u ||
      (report.duration_ms > 32767u && report.duration_ms != 0xffffu) ||
      report.trigger_repeat_ms > 32767u || report.sample_period_ms > 32767u ||
      report.start_delay_ms > 32767u) {
    return std::nullopt;
  }
  return report;
}

inline std::optional<DeviceControl> decode_device_control(const std::uint8_t* data,
                                                          std::size_t size) {
  if (data == nullptr || size != kDeviceControlPayloadSize || data[0] == 0u ||
      data[0] > 32u || (data[0] & static_cast<std::uint8_t>(data[0] - 1u)) != 0u) {
    return std::nullopt;
  }
  return static_cast<DeviceControl>(data[0]);
}

// Projects a polar PID direction onto this device's single X force axis.
// PID polar angles are clockwise from north, so +X/east is 90 degrees.
// Bhaskara's sine approximation is monotonic in each quadrant and exact at
// the cardinal directions. Its worst-case error is small relative to Q15.
inline std::int16_t direction_x_q15(const SetEffect& report) {
  if ((report.flags & kDirectionEnable) == 0u) {
    return (report.flags & kAxisXEnable) != 0u ? 32767 : 0;
  }

  const std::uint32_t direction = report.direction_hundredth_deg;
  const std::int32_t sign = direction < 18000u ? 1 : -1;
  const std::uint32_t sine_angle = direction <= 18000u ? direction : direction - 18000u;

  const std::int64_t product = static_cast<std::int64_t>(sine_angle) *
                               static_cast<std::int64_t>(18000u - sine_angle);
  const std::int64_t numerator = 4 * product * 32767;
  const std::int64_t denominator = 405000000 - product;
  const std::int32_t magnitude = static_cast<std::int32_t>(
      (numerator + denominator / 2) / denominator);
  return static_cast<std::int16_t>(sign * magnitude);
}

}  // namespace wheel::usb_pid_reports

