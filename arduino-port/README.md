# ESP32-S3 Arduino force-feedback wheel

> **Current build: passive-wheel mode.** Steering, two digital pedal switches,
> and both paddle buttons are active over USB. The full PID HID interface remains present, but motor
> actuation is compile-time disabled. GPIO42, GPIO41, and GPIO39 are held low
> regardless of host commands or saved settings. Set the wheel physically at
> center before powering it; without saved calibration, the first healthy
> AS5600 sample becomes the temporary center for that boot.

To save a new center without a PC utility, place the wheel at mechanical
center and hold both paddles together for two seconds. The axis immediately
snaps to zero and the AS5600 angle is saved to NVS. Release both paddles before
using the gesture again. Re-centering does not alter passive mode.

This is a fixed-hardware Arduino-ESP32 adaptation of
[ranenbg/Arduino-FFB-wheel](https://github.com/ranenbg/Arduino-FFB-wheel) for:

- ESP32-S3-WROOM-1 N16R8, generic 44-pin development board;
- one AS5600 absolute magnetic encoder over I2C;
- one L298N module, with both bridge channels commanded together;
- two normally-closed digital pedal switches for accelerator and brake;
- two digital paddle switches.

It keeps the standard DirectInput/PID effect model but replaces every
ATmega32U4-specific layer: ESP32-S3 native USB uses Arduino `USBHID`, the
AS5600 uses bounded I2C transactions with magnet validation, motor output uses
16 kHz LEDC PWM, and the paddles use debounced GPIO inputs. No patched Arduino
core or third-party AS5600 library is required.

Supported effects are constant, ramp, square, sine, triangle, sawtooth up and
down, spring, damper, friction, and inertia. The HID interface exposes one
signed 16-bit, 360-degree steering axis plus 32 buttons. The digital controls
use the common PlayStation DirectInput positions: GPIO48 brake is L1 (Button
5), GPIO38 accelerator is R1 (Button 6), GPIO14 is L2 (Button 7), and GPIO13
is R2 (Button 8).

The upstream Arduino-FFB GUI serial protocol is not part of this focused port.
Use `tools/wheel_config.py` for limits, direction, centering, faults, and live
status. Games still communicate through the standard USB PID reports.

## Pin map

| Function | ESP32-S3 GPIO | Connection |
|---|---:|---|
| Shared motor PWM | 42 | L298N ENA and ENB, enable jumpers removed |
| Motor direction 1 | 41 | L298N IN1 and IN3 |
| Motor direction 2 | 39 | L298N IN2 and IN4 |
| AS5600 SDA | 8 | I2C data, pulled up to 3.3 V |
| AS5600 SCL | 9 | I2C clock, pulled up to 3.3 V |
| Brake / L1 | 48 | Normally-closed switch contact to GND |
| Accelerator / R1 | 38 | Normally-closed switch contact to GND |
| Paddle up / R2 | 13 | Switch contact to GND |
| Paddle down / L2 | 14 | Switch contact to GND |
| Native USB D- / D+ | 19 / 20 | Board's USB-OTG connector |

GPIO10 is no longer used; remove the former encoder-index connection.

All four digital inputs use the ESP32-S3 internal pull-up. For each pedal,
connect the switch common terminal to GND and its NC terminal to the assigned
GPIO. A released pedal closes the contact and reads LOW; pressing it opens the
contact and reads HIGH. The paddle released levels are learned at startup so
their existing NO or NC wiring works; keep both paddles released whenever the
ESP32-S3 powers up or resets. Never connect a switch input to 5 V. For long or
noisy wires, add an external 4.7-10 kOhm resistor from each input GPIO to 3.3 V.

All grounds must be common. Do not apply 5 V to an ESP32-S3 GPIO. Add an
external pull-down (10 kOhm is typical) at each L298N enable input so the
bridges stay disabled during reset, flashing, or loss of ESP32 power.

Power the AS5600 breakout from 3.3 V and join its ground to ESP32 ground. SDA
and SCL require pull-ups to 3.3 V; many breakouts include pull-ups tied to the
module supply. Never allow either bus line to be pulled up to 5 V. The fixed
7-bit I2C address is `0x36`; AS5600 `OUT` is unused. Give `DIR` a defined
level--GND is recommended--and use the firmware `direction` setting if the
reported steering direction needs reversing.

The sensor supplies a 12-bit absolute angle (`0..4095`). Firmware unwraps the
`4095/0` crossing while powered and maps a half-turn either side of the saved
center to the signed HID range. The intended steering range is therefore 360
degrees. The AS5600 cannot recover a multi-turn position after reboot; a
900-degree wheel needs a separate homing/index mechanism or a multi-turn
encoder.

Follow the L298N module's logic-supply instructions, never power its logic rail
from an ESP32 GPIO, and do not use the module's 5 V regulator to back-power the
ESP32 board.

## Build

Use Arduino-ESP32 3.3.8. In Arduino IDE select **ESP32S3 Dev Module** and:

- USB Mode: **USB-OTG (TinyUSB)**
- USB CDC On Boot: **Disabled**
- Upload Mode: **UART0 / Hardware CDC**
- Flash Size: **16MB**
- PSRAM: **OPI PSRAM**

Open `arduino-port.ino`, compile, and flash through the board's programming
or UART connector. The USB-OTG connector becomes the dedicated wheel device.

Equivalent Arduino CLI commands:

```powershell
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.8 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,UploadMode=default,FlashSize=16M,PSRAM=opi" arduino-port
```

Do not connect motor power just because the firmware compiles or enumerates.
Follow [commissioning](docs/COMMISSIONING.md) first.

## Safe-start behavior

With `board::kPassiveWheelMode` set to `true`, the L298N never arms. The
following behavior applies only after intentionally changing that constant to
`false` and rebuilding.

On a fresh flash, force and PWM limits are zero and an invalid-configuration
fault is latched. The motor cannot arm until all of these are true:

- a valid configuration with nonzero force and duty limits is stored;
- the latest AS5600 I2C read succeeded and its magnet-detected bit is set;
- the encoder is nearly stationary;
- USB is mounted and not suspended;
- the host has enabled PID actuators;
- no fault is latched.

Status also exposes the AS5600 magnet-too-weak and magnet-too-strong flags.
Correct either warning before applying motor power. Changing from the older
MT6835 build bumps the saved-configuration schema, so the old record is safely
rejected and the force/duty limits must be configured again.

The L298N provides no current, voltage, or temperature telemetry. Firmware
cannot detect an overheated bridge, stalled motor, wiring fault, or pressed
hardwired emergency stop.

## Project origin

The target behavior was reviewed against Arduino-FFB-wheel commit
`463a08058d3fe6d955e7b1e50f12e483e279558f`. Its AVR USB core, timers, encoder
ISR, input matrix, and PWM implementation are not used here. See
`THIRD_PARTY_NOTICES.md` for descriptor provenance.
