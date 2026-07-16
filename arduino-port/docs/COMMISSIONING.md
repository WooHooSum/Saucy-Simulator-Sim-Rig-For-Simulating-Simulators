# Commissioning

The checked-in firmware is currently a passive-wheel build. Steering and all
four digital controls work, but the firmware never initializes PWM and continuously holds
ENA+ENB, IN1+IN3, and IN2+IN4 low. Leave `board::kPassiveWheelMode = true`
until active FFB commissioning is explicitly intended.

In passive mode, center the wheel and hold both paddles for two seconds to
capture and save the current AS5600 angle as zero. The gesture only triggers
once per hold; release both paddles before repeating it.

Keep motor power disconnected until step 11. Use a current-limited bench
supply and a physical emergency stop that interrupts the motor PSU.

1. Remove the L298N ENA/ENB jumpers. Confirm GPIO42 reaches both enables and
   each enable has an external pull-down. Confirm GPIO41 reaches IN1+IN3 and
   GPIO39 reaches IN2+IN4.
2. Power the AS5600 breakout from 3.3 V, connect SDA to GPIO8 and SCL to GPIO9,
   and join all logic grounds. Verify both I2C pull-ups terminate at 3.3 V,
   never 5 V. Leave AS5600 OUT disconnected and tie DIR to GND (or otherwise
   give it a defined logic level).
3. Wire the normally-closed brake switch between GPIO48 and GND, and the
   normally-closed accelerator switch between GPIO38 and GND. At rest each GPIO
   must read LOW; pressing the pedal opens the contact and the internal pull-up
   makes it read HIGH. Never connect either switch input to 5 V.
4. Flash through UART/programming USB. Connect the separate native USB-OTG
   port to the PC and confirm `1209:0001` enumerates as a game controller.
5. Install the configuration tool dependency:

   ```powershell
   py -m pip install -r tools/requirements.txt
   ```

6. With motor power still disconnected, inspect the encoder and all buttons:

   ```powershell
   py tools/wheel_config.py status
   ```

   `encoder_io_ok` and `encoder_ready` must be true, `magnet_detected` must be
   true, and `encoder_io_errors` must stop increasing. Correct magnet spacing
   if `magnet_too_weak` or `magnet_too_strong` is true. The `buttons` mask must
   show `0x10` for GPIO48 brake/L1, `0x20` for GPIO38 accelerator/R1, `0x40`
   for GPIO14/L2, and `0x80` for GPIO13/R2. The programming-port trace must
   show `brake48` and `accel38` changing from 0 to 1 when pressed.
7. Rotate the wheel through one revolution. `raw_angle` must cover `0..4095`
   and wrap cleanly while `encoder_position` remains continuous.
8. Put the wheel at its mechanical center and store that single-turn angle:

   ```powershell
   py tools/wheel_config.py capture-zero
   ```

9. Check Windows steering and the L1, R1, L2, and R2 buttons. If steering
   direction is reversed, store
   `--direction -1` (or restore `1`).
10. With motor power still off, store initial limits of only five percent:

   ```powershell
   py tools/wheel_config.py set --maximum-force 0.05 --maximum-duty 0.05
   py tools/wheel_config.py clear-fault
   py tools/wheel_config.py status
   ```

11. Reset the board several times and measure ENA/ENB. They must remain low
    during reset and USB enumeration. Check GPIO41/GPIO39 and GPIO42 PWM with an
    oscilloscope or current-limited indicator load. Verify the hardwired
    emergency stop actually removes motor PSU power without firmware.
12. Connect one motor at low voltage and a strict current limit. Test a weak
    constant force, then a weak spring. If a spring drives away from center,
    cut power immediately and reverse the motor output leads or encoder
    direction; do not compensate by increasing limits.
13. If using both L298N bridge outputs for two motors, connect the second motor
    and confirm both pull the belt in the same direction. Reverse only the
    second motor's leads if they fight.
14. Test USB disconnect, AS5600 disconnect, magnet removal, pedal and paddle inputs,
    fault clearing, and the power-cut emergency stop.
15. Increase limits slowly while externally measuring motor current and L298N,
    motor, connector, and wiring temperatures.

Use `status` after any failure. Fault numbers and names are defined by
`FaultCode` in `src/wheel/control_protocol.hpp`. A configuration update always
disarms the motor; the game must enable actuators again. The AS5600 cannot
recover multiple wheel turns after reboot, so this build is limited to a
single 360-degree steering range.

## Physical protection

At minimum use a motor-supply fuse, current-limited supply for bring-up,
adequate L298N heatsinking/airflow, bulk capacitance, flyback protection,
enable pull-downs, a latching PSU-side emergency stop, and mechanical stops
that can survive maximum available torque.

The USB VID/PID is a private test identity and must be replaced with an ID you
are authorized to use before distributing hardware or firmware.
