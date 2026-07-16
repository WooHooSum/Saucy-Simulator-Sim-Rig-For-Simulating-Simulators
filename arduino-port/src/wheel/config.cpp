#include "config.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace wheel {
namespace {

std::uint32_t crc32_bytes(const std::uint8_t* data, std::size_t size) {
  std::uint32_t crc = 0xffffffffu;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (unsigned bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1u) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

bool unit_float(float value) { return std::isfinite(value) && value >= 0.0F && value <= 1.0F; }

}  // namespace

std::uint32_t config_crc32(const ConfigRecord& record) {
  return crc32_bytes(reinterpret_cast<const std::uint8_t*>(&record),
                     offsetof(ConfigRecord, crc32));
}

bool config_valid(const ConfigRecord& record) {
  const auto& cfg = record.runtime;
  return record.magic == kConfigMagic &&
         record.schema_version == kConfigSchemaVersion &&
         record.length == sizeof(ConfigRecord) &&
         (cfg.encoder.direction == 1 || cfg.encoder.direction == -1) &&
         cfg.encoder.zero_offset >= 0 && cfg.encoder.zero_offset <= 4095 &&
         std::isfinite(cfg.encoder.velocity_alpha) &&
         cfg.encoder.velocity_alpha > 0.0F && cfg.encoder.velocity_alpha <= 1.0F &&
         unit_float(cfg.global_gain) && unit_float(cfg.maximum_force) &&
         unit_float(cfg.maximum_duty) &&
         std::isfinite(cfg.duty_slew_per_tick) && cfg.duty_slew_per_tick > 0.0F &&
         cfg.duty_slew_per_tick <= 1.0F && record.crc32 == config_crc32(record);
}

void config_finalize(ConfigRecord& record) {
  record.magic = kConfigMagic;
  record.schema_version = kConfigSchemaVersion;
  record.length = sizeof(ConfigRecord);
  record.crc32 = config_crc32(record);
}

ConfigRecord safe_default_config() {
  ConfigRecord record{};
  config_finalize(record);
  return record;
}

}  // namespace wheel
