# travelers-board-split (central bring-up)

This independent ZMK v0.3 configuration targets the available master/central
PCB with a u-blox NINA-B302-00B (nRF52840). It deliberately does not assume
any peripheral PCB, left/right symmetry, USB hardware, a bootloader, or a
SoftDevice. The build is central-only; a future BLE-split configuration can be
added when the peripheral's wiring is known.

The project/board name remains `travelers-board-split`; its advertised BLE
name is `travelers-split`, because ZMK v0.3 limits that name to 16 bytes.

## Build

From the workspace root:

```sh
just init config/travelers-board-split
just build travelers-board-split-central -p
```

The sole SWD artifact is `firmware/travelers-board-split-central.hex`. UF2
remains enabled for other workspace configurations, but this target
intentionally builds no UF2 and has no USB support.

## Current bring-up scope

- BLE HID and the local 5 x 6 `col2row` matrix are enabled. The matrix
  transform exposes the 26 electrically populated positions: three on row 3
  and five on row 4, following the physical PCB layout.
- Flash code starts at `0x00000000`; settings/NVS occupy `0x000e0000` through
  `0x000fffff`. Confirm these values again from the generated map, DTS, HEX,
  and `.config` after every toolchain/ZMK change.
- The NINA-B302's onboard 32 MHz and 32.768 kHz crystals are used. The module
  owns their load network; no board-level capacitance or UICR/regulator change
  is made.
- Battery reporting is enabled using a local Zephyr module driver, not a ZMK
  source modification. It enables P0.13, waits 300 ms for the switched 1 Mohm
  / 1 Mohm divider and 100 nF capacitor to settle, samples P0.04 / SAADC AIN2,
  then turns P0.13 off. The result is sent through the BLE Battery Service.
  `empty-millivolts` (2.0 V) and `full-millivolts` (3.0 V) are an initial
  linear CR2032 loaded-VDD estimate and can be calibrated in the board DTS.

## OpenOCD SWD programming

Remove the CR2032 before connecting the probe. Power the target only from the
debugger, then connect VDD, SWDIO, SWCLK, and common GND. Do not connect the
coin cell and debugger target power together.

Start OpenOCD using the configuration for the actual probe (for example,
`interface/stlink.cfg` only when using an ST-Link):

```sh
openocd -f <debugger-interface>.cfg -f target/nrf52.cfg
```

From `telnet localhost 4444`, program the generated Intel HEX without an
offset; its addresses are link-time addresses:

```text
reset halt
program <absolute-path>/firmware/travelers-board-split-central.hex verify reset
shutdown
```

Do not program an S140 SoftDevice or a bootloader HEX. A mass erase also erases
ZMK settings and BLE bonds; remove the host pairing before pairing again.
