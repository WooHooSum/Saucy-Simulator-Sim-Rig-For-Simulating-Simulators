#pragma once

#include <cstdint>

#include "USBHID.h"
#include "shared_state.hpp"
#include "src/wheel/config.hpp"

namespace usb_ffb {

inline constexpr std::uint8_t kWheelReportId = 0x01;
inline constexpr std::uint8_t kStateReportId = 0x02;
inline constexpr std::uint8_t kConfigReportId = 0x20;
inline constexpr std::uint8_t kStatusReportId = 0x21;

void initialize(RuntimeShared& shared, const wheel::ConfigRecord& config);
void service();
void set_report(std::uint8_t report_id, hid_report_type_t report_type,
                const std::uint8_t* data, std::uint16_t size);
std::uint16_t get_report(std::uint8_t report_id, hid_report_type_t report_type,
                         std::uint8_t* data, std::uint16_t capacity);
void mounted(bool value);
void suspended(bool value);

}  // namespace usb_ffb
