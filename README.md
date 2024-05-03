# beosound5 HID kernel driver
Linux kernel module for the Beosound 5 from Bang & Olufsen

This module handles the following

## Inputs

- wheel inputs (laser, selection, volume)
- buttons (left, right, go, mode)

The inputs are exposed as input device events. You can check them with `evtest`

## Outputs

- indicator led
- screeen backlight (on/off)

These two "leds" are exposed using Linux's led subsystem in:

- /sys/class/leds/beosound5::indicator
- /sys/class/leds/beosound5::backlight

## Build

To build this module run:

```bash
make
```

## Test

To test this module you can use this scripts:

```bash
./setup.sh    - replace default Linux USB keyboard modules with current release
./restore.sh  - restore
./test.sh     - timeout testing
```
