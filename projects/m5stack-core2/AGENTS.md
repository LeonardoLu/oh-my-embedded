# M5Stack Core2 device

- Read `specs/hardware.md` and the selected project's AGENTS before changing code.
- This device directory contains independent Codex and PPS firmware. BLE HID,
  Bottom2 LEDs and companion behavior are Codex-project rules, not device defaults.
- Keep assembly distinctions explicit: Codex uses Battery Bottom2 v1.3; PPS uses
  Module13.2 PPS and Base Bottom v1.1. Do not conflate the two bottoms.
- The Codex baseline names Core2 v1.3; PPS hardware evidence reports AXP192.
  Do not silently treat these as a confirmed identical board revision or infer
  the number of physical devices. Record observed revision/PMIC per validation.
- Display, power and peripheral initialization belong to the selected firmware.
  In particular, do not copy Codex LED/audio power assumptions into PPS.
