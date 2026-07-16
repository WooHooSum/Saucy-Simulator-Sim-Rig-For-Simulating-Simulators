#!/usr/bin/env python3
"""Configuration and status utility for the Arduino ESP32-S3 FFB wheel."""

from __future__ import annotations

import argparse
import dataclasses
import struct
import sys
import zlib

try:
    import hid
except ModuleNotFoundError:
    hid = None

VID = 0x1209
PID = 0x0001
CONFIG_REPORT = 0x20
STATUS_REPORT = 0x21
PAYLOAD_SIZE = 63
MAGIC = 0x57464642
SCHEMA = 4
CONFIG_STRUCT = struct.Struct("<IHHb3xifffffI")
STATUS_STRUCT = struct.Struct("<BBifhhHHIIIHBBBBB")
assert STATUS_STRUCT.size == 37

FAULT_NAMES = {
    0: "none",
    1: "encoder_failure",
    2: "magnet_missing",
    3: "encoder_implausible_motion",
    4: "control_overrun",
    5: "usb_command_overflow",
    6: "invalid_configuration",
    7: "internal_error",
}
STATE_NAMES = {0: "boot", 1: "disarmed", 2: "armed", 3: "fault"}


@dataclasses.dataclass
class Config:
    direction: int = 1
    zero_offset: int = 0
    velocity_alpha: float = 0.20
    global_gain: float = 1.0
    maximum_force: float = 0.0
    maximum_duty: float = 0.0
    duty_slew_per_tick: float = 0.002

    def pack(self) -> bytes:
        prefix = CONFIG_STRUCT.pack(
            MAGIC, SCHEMA, CONFIG_STRUCT.size, self.direction, self.zero_offset,
            self.velocity_alpha, self.global_gain, self.maximum_force,
            self.maximum_duty, self.duty_slew_per_tick, 0,
        )
        crc = zlib.crc32(prefix[:-4]) & 0xFFFFFFFF
        return prefix[:-4] + struct.pack("<I", crc)

    @classmethod
    def unpack(cls, data: bytes) -> "Config":
        values = CONFIG_STRUCT.unpack(data[: CONFIG_STRUCT.size])
        magic, schema, length = values[:3]
        if (magic, schema, length) != (MAGIC, SCHEMA, CONFIG_STRUCT.size):
            raise ValueError("unsupported configuration header")
        if zlib.crc32(data[: CONFIG_STRUCT.size - 4]) & 0xFFFFFFFF != values[-1]:
            raise ValueError("configuration CRC mismatch")
        return cls(*values[3:-1])


@dataclasses.dataclass(frozen=True)
class Status:
    version: int
    length: int
    encoder_position: int
    encoder_velocity: float
    hid_position: int
    force_command: int
    raw_angle: int
    loop_time_us: int
    tick_count: int
    buttons: int
    encoder_io_errors: int
    first_fault: int
    state: int
    encoder_io_ok: int
    magnet_status: int
    effects_playing: int
    paused: int

    @property
    def angle_degrees(self) -> float:
        return self.hid_position * 180.0 / 32767.0

    @property
    def raw_angle_degrees(self) -> float:
        return self.raw_angle * 360.0 / 4096.0

    @property
    def magnet_detected(self) -> bool:
        return bool(self.magnet_status & 0x20)

    @property
    def magnet_too_weak(self) -> bool:
        return bool(self.magnet_status & 0x10)

    @property
    def magnet_too_strong(self) -> bool:
        return bool(self.magnet_status & 0x08)

    @property
    def encoder_ready(self) -> bool:
        return bool(self.encoder_io_ok) and self.magnet_detected


def device() -> hid.device:
    if hid is None:
        raise RuntimeError("missing hidapi; run: python3 -m pip install -r tools/requirements.txt")
    result = hid.device()
    result.open(VID, PID)
    return result


def get_feature(dev: hid.device, report_id: int) -> bytes:
    data = bytes(dev.get_feature_report(report_id, 64))
    if not data:
        raise RuntimeError(f"feature report 0x{report_id:02x} returned no data")
    return data[1:] if data[0] == report_id else data


def set_feature(dev: hid.device, payload: bytes) -> None:
    if len(payload) > PAYLOAD_SIZE:
        raise ValueError("feature payload too large")
    report = bytes([CONFIG_REPORT]) + payload.ljust(PAYLOAD_SIZE, b"\0")
    if dev.send_feature_report(report) != len(report):
        raise RuntimeError("short feature-report write")


def read_config(dev: hid.device) -> Config:
    return Config.unpack(get_feature(dev, CONFIG_REPORT))


def read_status(dev: hid.device) -> Status:
    payload = get_feature(dev, STATUS_REPORT)
    values = STATUS_STRUCT.unpack(payload[: STATUS_STRUCT.size])
    status = Status(*values)
    if status.version != 4 or status.length != STATUS_STRUCT.size:
        raise ValueError("unsupported status-report version or length")
    return status


def show_config(config: Config) -> None:
    for field in dataclasses.fields(config):
        print(f"{field.name}: {getattr(config, field.name)}")


def show_status(dev: hid.device) -> None:
    status = read_status(dev)
    for field in dataclasses.fields(status):
        print(f"{field.name}: {getattr(status, field.name)}")
    print(f"angle_degrees: {status.angle_degrees:.2f}")
    print(f"raw_angle_degrees: {status.raw_angle_degrees:.2f}")
    print(f"magnet_detected: {status.magnet_detected}")
    print(f"magnet_too_weak: {status.magnet_too_weak}")
    print(f"magnet_too_strong: {status.magnet_too_strong}")
    print(f"encoder_ready: {status.encoder_ready}")
    print(f"fault_name: {FAULT_NAMES.get(status.first_fault, 'unknown')}")
    print(f"state_name: {STATE_NAMES.get(status.state, 'unknown')}")


def main() -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("config")
    sub.add_parser("status")
    sub.add_parser("capture-zero")
    sub.add_parser("clear-fault")
    update = sub.add_parser("set")
    update.add_argument("--direction", type=int, choices=(-1, 1))
    update.add_argument("--zero-offset", type=int)
    update.add_argument("--velocity-alpha", type=float)
    update.add_argument("--global-gain", type=float)
    update.add_argument("--maximum-force", type=float)
    update.add_argument("--maximum-duty", type=float)
    update.add_argument("--duty-slew-per-tick", type=float)
    args = parser.parse_args()

    try:
        dev = device()
        if args.command == "config":
            show_config(read_config(dev))
        elif args.command == "status":
            show_status(dev)
        elif args.command == "capture-zero":
            if not read_status(dev).encoder_ready:
                raise RuntimeError("AS5600 I2C and magnet must be healthy before capture-zero")
            set_feature(dev, b"\x03")
            print("center captured; configuration saved and motor disarmed")
        elif args.command == "clear-fault":
            set_feature(dev, b"\x02")
            print("fault-clear request sent")
        elif args.command == "set":
            config = read_config(dev)
            for field in dataclasses.fields(config):
                value = getattr(args, field.name)
                if value is not None:
                    setattr(config, field.name, value)
            if not 0.0 < config.velocity_alpha <= 1.0:
                raise ValueError("velocity-alpha must be in (0, 1]")
            if not 0 <= config.zero_offset <= 4095:
                raise ValueError("zero-offset must be in [0, 4095] for the AS5600")
            if not 0.0 < config.duty_slew_per_tick <= 1.0:
                raise ValueError("duty-slew-per-tick must be in (0, 1]")
            for value in (config.global_gain, config.maximum_force, config.maximum_duty):
                if not 0.0 <= value <= 1.0:
                    raise ValueError("gain/force/duty limits must be in [0, 1]")
            set_feature(dev, b"\x01" + config.pack())
            show_config(config)
            print("configuration saved; motor disarmed")
        dev.close()
        return 0
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
