#pragma once

#include <cstddef>
#include <cstdint>

namespace ffb_hid {

bool begin();
bool ready();
bool send_report(std::uint8_t report_id, const void* data, std::size_t size);

}  // namespace ffb_hid
