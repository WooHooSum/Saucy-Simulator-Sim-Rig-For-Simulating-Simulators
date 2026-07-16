#include "ffb_hid_device.hpp"

#include <cstdint>
#include <cstring>

#include "USBHID.h"
#include "usb_pid.hpp"
#include "usb_pid_compat.hpp"
#include "usb_pid_descriptor_data.hpp"

namespace {

USBHID g_hid;

class ForceFeedbackHidDevice final : public USBHIDDevice {
 public:
  ForceFeedbackHidDevice()
      : registered_(g_hid.addDevice(this, kHid1FfbDescSize)) {}

  bool begin() {
    if (!registered_) return false;
    g_hid.begin();
    return true;
  }

  std::uint16_t _onGetDescriptor(std::uint8_t* buffer) override {
    if (buffer == nullptr) return 0;
    std::memcpy(buffer, hid_1ffb_desc, kHid1FfbDescSize);
    return kHid1FfbDescSize;
  }

  std::uint16_t _onGetFeature(std::uint8_t report_id, std::uint8_t* buffer,
                              std::uint16_t length) override {
    return usb_ffb::get_report(report_id, HID_REPORT_TYPE_FEATURE, buffer, length);
  }

  void _onSetFeature(std::uint8_t report_id, const std::uint8_t* buffer,
                     std::uint16_t length) override {
    // Arduino-ESP32 routes control-endpoint SET_REPORT requests here even for
    // PID output reports. Only these two IDs are writable feature reports.
    const hid_report_type_t type =
        (report_id == HID_ID_NEWEFREP || report_id == usb_ffb::kConfigReportId)
            ? HID_REPORT_TYPE_FEATURE
            : HID_REPORT_TYPE_OUTPUT;
    usb_ffb::set_report(report_id, type, buffer, length);
  }

  void _onOutput(std::uint8_t report_id, const std::uint8_t* buffer,
                 std::uint16_t length) override {
    usb_ffb::set_report(report_id, HID_REPORT_TYPE_OUTPUT, buffer, length);
  }

 private:
  bool registered_{false};
};

ForceFeedbackHidDevice g_device;

}  // namespace

namespace ffb_hid {

bool begin() { return g_device.begin(); }

bool ready() { return g_hid.ready(); }

bool send_report(std::uint8_t report_id, const void* data, std::size_t size) {
  return g_hid.SendReport(report_id, data, size, 5);
}

}  // namespace ffb_hid
