# Garage Door 32

DIY Automatic Garage Door Closer

This Wi-Fi device will automatically close the garage door if it is left open for too long. Its Web interface cannot be used to open the garage door remotely. 

## Features

- Based on an ESP32 module (Lolin32_Lite)

- Suitable for garage door openers that can be operated by shorting two terminals.

- The time the door is left open depends on the temperature and the light level in the garage.

- A buzzer is activated at regular intervals when the door is open and while the relay is activated.

- The automatic door closure can be suspended or resumed with a button press.

## Third Party Libraries

Six libraries are used. The [PlatformIO configuration file](platformio.ini) lists them. Many thanks to the authors and contributors for sharing their work. Without these libraries, this device could not exist.

## Components

- Microprocessor: Lolin32 Lite

- Relay: ubiquitous 3.3V compatible low power relay module

- Contact switch: generic door/window contact switch 

- Light sensor: light-dependent resistor (LDR)

- Temperature and humidity sensor: I²C DHT20

- Buzzer: active 5V buzzer driven with 2N3904 NPN transistor

- Remote: push button switch and RGB LED (they require 4-wire cable)

- Various pull-up and current limiting resistors

There are provisions for a status LED and a second microprocessor for Bluetooth connectivity but the code does not support them yet.

## Circuit and Layout

[Circuit](img/circuit2.png) and [component layout](img/layout.png)  diagrams are available showing the current version of the hardware.

## Status

The device has been functioning for about four weeks. Currently the automatic closure remote, the status LED, and the Bluetooth controller are not in place. The automatic closure remote was tested when breadboarding the circuit and it does work, but it can't be deployed until the wiring through the wall and ceiling is completed.

The firwmare needs much more testing in the field, consider this to an alpha release and use at your own risk.

A future hardware revision could include a second contact switch (to I/O port 35) to verify that the door is fully open. It is not obvious that this would be worthwhile.

## Licence

The **BSD Zero Clause** ([SPDX](https://spdx.dev/): [0BSD](https://spdx.org/licenses/0BSD.html)) licence applies.
