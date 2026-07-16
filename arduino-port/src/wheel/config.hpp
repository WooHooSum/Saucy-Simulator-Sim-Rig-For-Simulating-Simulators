#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace wheel {

inline constexpr std::uint32_t kConfigMagic = 0x57464642u;  // WFFB
inline constexpr std::uint16_t kConfigSchemaVersion = 4u;

struct EncoderConfig {
  std::int8_t direction{1};
  std::uint8_t reserved[3]{};
  std::int32_t zero_offset{0};
  float velocity_alpha{0.20F};
};

// Fixed-purpose configuration. Voltage and temperature fields are deliberately
// absent: this hardware has neither sensor and the firmware must not imply it
// can observe L298N health.
struct RuntimeConfig {
  EncoderConfig encoder{};
  float global_gain{1.0F};
  float maximum_force{0.0F};
  float maximum_duty{0.0F};
  float duty_slew_per_tick{0.002F};
};

struct ConfigRecord {
  std::uint32_t magic{kConfigMagic};
  std::uint16_t schema_version{kConfigSchemaVersion};
  std::uint16_t length{sizeof(ConfigRecord)};
  RuntimeConfig runtime{};
  std::uint32_t crc32{0};
};

static_assert(std::is_trivially_copyable_v<ConfigRecord>);

std::uint32_t config_crc32(const ConfigRecord& record);
bool config_valid(const ConfigRecord& record);
void config_finalize(ConfigRecord& record);
ConfigRecord safe_default_config();

}  // namespace wheel
