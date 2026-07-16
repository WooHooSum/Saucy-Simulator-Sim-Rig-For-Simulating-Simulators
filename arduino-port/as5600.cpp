#include "as5600.hpp"

#include <array>
#include <cstdint>

#include "board_config.hpp"
#include "esp_err.h"

bool AxisEncoder::initialize() {
  i2c_master_bus_config_t bus_config{};
  bus_config.i2c_port = I2C_NUM_0;
  bus_config.sda_io_num = board::kI2cSda;
  bus_config.scl_io_num = board::kI2cScl;
  bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_config.glitch_ignore_cnt = 7;
  bus_config.flags.enable_internal_pullup = false;
  if (i2c_new_master_bus(&bus_config, &bus_) != ESP_OK) return false;

  i2c_device_config_t device_config{};
  device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  device_config.device_address = board::kAs5600Address;
  device_config.scl_speed_hz = board::kI2cFrequencyHz;
  return i2c_master_bus_add_device(bus_, &device_config, &device_) == ESP_OK;
}

AxisEncoder::Sample AxisEncoder::read() {
  Sample sample{};
  sample.io_error_count = io_error_count_;
  if (device_ == nullptr) return sample;

  // AS5600 STATUS (0x0b), RAW ANGLE high (0x0c), RAW ANGLE low (0x0d).
  constexpr std::uint8_t kStatusRegister = 0x0b;
  std::array<std::uint8_t, 3> bytes{};
  if (i2c_master_transmit_receive(device_, &kStatusRegister, 1, bytes.data(),
                                  bytes.size(), 1) != ESP_OK) {
    if (io_error_count_ != UINT32_MAX) ++io_error_count_;
    sample.io_error_count = io_error_count_;
    return sample;
  }

  sample.status = bytes[0];
  sample.raw_angle = static_cast<std::uint16_t>(
      ((bytes[1] & 0x0fu) << 8u) | bytes[2]);
  sample.io_error_count = io_error_count_;
  sample.io_ok = true;
  return sample;
}
