#pragma once

#include <cstdint>

#include "driver/i2c_master.h"

class AxisEncoder {
 public:
  struct Sample {
    std::uint16_t raw_angle{0};
    std::uint8_t status{0};
    std::uint32_t io_error_count{0};
    bool io_ok{false};

    bool magnet_detected() const { return (status & 0x20u) != 0u; }
    bool reference_valid() const { return io_ok && magnet_detected(); }
  };

  bool initialize();
  Sample read();

 private:
  i2c_master_bus_handle_t bus_{nullptr};
  i2c_master_dev_handle_t device_{nullptr};
  std::uint32_t io_error_count_{0};
};
