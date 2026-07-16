#pragma once

#include <cstdint>

// Descriptor definitions derived from the MIT-licensed OpenFFBoard descriptor.
// See THIRD_PARTY_NOTICES.md.
#define AXIS1_FFB_HID_DESC 1
#define MAX_AXIS 1
#define MAX_EFFECTS 40
#define FFB_ID_OFFSET 0

#define HID_USAGE_DESKTOP_X 0x30
#define HID_USAGE_DESKTOP_Y 0x31
#define HID_USAGE_DESKTOP_Z 0x32
#define HID_USAGE_DESKTOP_RX 0x33
#define HID_USAGE_DESKTOP_RY 0x34
#define HID_USAGE_DESKTOP_RZ 0x35
#define HID_USAGE_DESKTOP_SLIDER 0x36
#define HID_USAGE_DESKTOP_DIAL 0x37

#define HID_USAGE_CONST 0x26
#define HID_USAGE_RAMP 0x27
#define HID_USAGE_SQUR 0x30
#define HID_USAGE_SINE 0x31
#define HID_USAGE_TRNG 0x32
#define HID_USAGE_STUP 0x33
#define HID_USAGE_STDN 0x34
#define HID_USAGE_SPRNG 0x40
#define HID_USAGE_DMPR 0x41
#define HID_USAGE_INRT 0x42
#define HID_USAGE_FRIC 0x43

#define HID_ID_STATE 0x02
#define HID_ID_EFFREP 0x01
#define HID_ID_ENVREP 0x02
#define HID_ID_CONDREP 0x03
#define HID_ID_PRIDREP 0x04
#define HID_ID_CONSTREP 0x05
#define HID_ID_RAMPREP 0x06
#define HID_ID_CSTMREP 0x07
#define HID_ID_SMPLREP 0x08
#define HID_ID_EFOPREP 0x0A
#define HID_ID_BLKFRREP 0x0B
#define HID_ID_CTRLREP 0x0C
#define HID_ID_GAINREP 0x0D
#define HID_ID_SETCREP 0x0E
#define HID_ID_NEWEFREP 0x11
#define HID_ID_BLKLDREP 0x12
#define HID_ID_POOLREP 0x13
#define HID_ID_HIDCMD 0xA1

#define __ALIGN_BEGIN
#define __ALIGN_END __attribute__((aligned(4)))

