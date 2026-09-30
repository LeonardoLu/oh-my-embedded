# StopWatch clock continuity

2026-09-13 correction for RTC handling across panel sleep, ESP32 light sleep and
M5PM1 double-click shutdown.

## Contract

The RX8130CE is the sole wall-clock source. Panel sleep and ESP32 light sleep do
not write its calendar. Display invalidation forces a fresh read on the restored
frame. No boot path seeds the RTC from firmware compilation time.

`RtcClock` reads the RX8130 calendar in one seven-byte transaction and checks VLF
before and after it. It validates BCD, the weekday register, seconds, month length
and leap years, accepting the editor's 2020–2099 range. A bus failure preserves the
last good snapshot and retries on later reads; this snapshot is frozen, not an
independently advancing clock. Failed initial RTC discovery retries at most once
per second. Invalid calendar data or an asserted VLF invalidates the snapshot.
The face and settings then show dashes instead of a fabricated time or date.

Unknown clocks require explicit date and time confirmation, in either order. The
first editor's Done opens the other editor; no hardware write happens until both
are confirmed. Cancel/Home cancels the pending confirmation. For a healthy RTC,
editing either half preserves the freshly read other half. Failed writes keep the
editor open and show the existing localized Error label.

All explicit full writes use the driver's boolean result and a calendar readback,
allowing up to one second of progression including across midnight. Only after
verification does the app clear VLF, using flag-register value `0xbd` to preserve
unrelated write-zero-to-clear flags. A final flag/calendar read must succeed.
The M5Unified `getVoltLow()` API tests VBLF (`0x80`), not VLF (`0x02`), so it is not
used as a substitute for oscillation validity.

## Double-click power off

`Power::update()` ensures and verifies M5PM1 `PWR_CFG` (`0x06`) LDO_EN bit 2 and
`HOLD_CFG` (`0x07`) LDO-hold bit 5. Read-modify-write preserves LED, charge, other
rails and other hold bits. The normal one-second power poll retries a failed
configuration and checks it remains present. Single-click Home, hardware double
off and download settings retain their previous behavior.

The StopWatch schematic connects RTC VDD to `3V3_L1` and VBAT to a 10 µF capacitor,
not an independent backup cell. Keeping L1 on also powers the IMU; shutdown power
consumption is consequently higher than L0-only shutdown. No battery-life claim
is made. Battery depletion or physical battery disconnection can still lose time.

References:

- [Official StopWatch power documentation](https://docs.m5stack.com/en/arduino/stopwatch/m5pm1_m5ioe1)
- [Official schematic, sheet 4](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/C152-SCH_Stopwatch_PRJ_Main_VA_20251201_2026_04_24_17_46_22.pdf)
- [Factory PMIC initialization also enables LDO hold](https://github.com/m5stack/M5StopWatch-UserDemo/blob/main/main/hal/hal_pmic.cpp)
- [RX8130CE register manual, section 14.5](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1132/RX8130CE_cn-Register-Datasheet.pdf)

## Diagnostics

- `rtc`: read success, snapshot validity, state (0 unset, 1 ready, 2 read error),
  raw flag register, date/time, power-hold readiness and screen power state.
- `rtc set YYYY-MM-DDTHH:MM:SS`: explicit local wall-time setting with validation
  and verified write; reports `RTC set=1` only on success. No timezone conversion
  or network synchronization occurs in firmware.
- `tools/sync_rtc.py` (host helper): captures host local time, sends `rtc set`
  and optionally reads the clock back to report skew. It retries briefly, but a
  dozed Keys-only watch drops all serial input while asleep, so the watch must
  be awake (touch or button wake) for a reply.
- `power`: additionally reports raw `hold_cfg`, `rtc_hold` and LDO enable.

## Validation

`sh tools/check_host.sh` passed, including native BotUx rendering and orb checks.
After the final RTC/retention changes, their focused host tests were recompiled
and passed. Fault cases cover boot/runtime read errors without writes, retained
snapshots, VLF asserted on a plausible date or during a read, invalid calendar,
write failure, ignored write, flag-clear failure, unrelated flag preservation,
both editor orders, cancellation, leap day, date preservation across midnight,
and failed RTC discovery followed by recovery. Retention tests check unrelated
bits survive, verified writes, failure/retry and no repeated writes when configured.

Final PlatformIO build: RAM 50,164 bytes; flash 1,095,157 bytes.
Firmware binary: 1,095,568 bytes, SHA-256
`301cc209c7ffcfa97f9e1c0dcdaca8144fc1f2c5209c20cedc0f5065ae89f5e7`.
`git diff --check` passed.

The user authorized flashing. The connected device identified itself through the
existing Watch UI/power serial commands; USB serial/MAC was `28:84:85:44:5B:8C`,
VID:PID `303A:1001`, port `/dev/cu.usbmodem214201`. PlatformIO uploaded through that
explicit port, verified the written regions and reset the ESP32-S3 successfully.
NVS was not erased.

Post-upload RTC calibration and register readback are recorded below. Physical
double-click shutdown retention duration and battery current remain unmeasured;
successful upload and LDO-hold readback alone do not establish those properties.

At 20:07:09 Asia/Shanghai, the initial device read returned `flags=27`, `valid=0`,
`state=0`: the hardware VLF was already set, and the new firmware rejected that
clock instead of displaying it or seeding compilation time. An explicit host
calibration returned `RTC set=1`. The flags became `25` (VLF cleared, unrelated
flags preserved). Reads advanced from `2026-09-13T20:07:09` to `20:07:13`.
M5PM1 readback was `hold_cfg=20 rtc_hold=1 ldo=1`, while button configurations
remained `key_cfg=2b off_cfg=00` and the LED stayed off.

After a separate esptool ESP32 reset, the device returned
`2026-09-13T20:08:27`, `read=1 valid=1 state=1 flags=25`, with LDO and hold still
enabled. The read matched host Beijing time within the bounded serial sampling
window. This establishes RTC continuity across that MCU reset, not physical
PMIC shutdown. The current screen remained the face and persisted sound,
language and button-feedback preferences remained intact.

A native 466×466 framebuffer capture after deployment showed a legible
`20:08:49` and `2026/09/13 星期日`, confirming the restored face rendered the
calibrated calendar. This is firmware framebuffer evidence, not a panel photo.
