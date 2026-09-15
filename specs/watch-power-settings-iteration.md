# StopWatch display, sound, and power settings iteration

2026-09-16 implementation contract for the expanded StopWatch settings. This
iteration supersedes the settings shape in `watch-power-experience.md`; its
audio suspension, panel sleep, input-consumption, RTC continuity, and hardware
validation boundaries still apply.

## Settings hierarchy

The former combined Display & Sound page is split into three top-level pages:

- Display: normal brightness, theme, PM status indicator, physical-button
  visual effect, bot-text visibility, and which face block appears at the top.
- Sound: master sound, startup sound, button sound, and alert/effect sound.
- Power: power-save enable, dim brightness, dim timeout, display-off timeout,
  wake mode, keep-awake while charging, forced-sleep enable, forced start hour,
  and forced end hour.

Normal and dim brightness are both five-level values. The effective dim level
is clamped to the normal level, so entering Dimmed can never make the display
brighter. The shipped values remain normal level 3, dim level 1, dim after 15
seconds, display off after one minute, Touch + keys wake, and no saving while
external power is present (`Charging keep awake` defaults On).

The master Sound switch gates every cue and the speaker transport. Startup,
button, and alert switches are independent subcategories and default on.
Button sounds include selection, confirmation, navigation, and back cues.
Alert/effect sounds include bot poke and error cues. The startup switch controls
the boot Wake cue. A subcategory never bypasses the master switch, and dim or
display-off still suspends the complete transport and powers down audio
hardware.

## Power policy

Power Save defaults on and is the prerequisite for both ordinary idle saving
and forced sleep. Turning it off keeps the panel Active and refreshes the idle
epoch. Turning it back on therefore starts complete dim and off intervals.
Long-B manual doze is available only while one of those policies applies.

The Charging keep-awake setting means external VBUS/VIN, not the narrower
active battery charge signal. Its default On preserves the established
plugged-in behavior by skipping ordinary idle dim and display-off. It also
covers a full battery whose charger has stopped while VBUS remains connected.
Turning it Off lets the ordinary dim/off timers apply on external power.

Forced sleep is disabled by default. When enabled, its start hour is inclusive
and end hour exclusive. `23 -> 7` therefore covers 23:00 through 06:59, and
ordinary same-day ranges such as `8 -> 18` are supported. Equal endpoints form
an empty range to avoid an accidental all-day lockout. The schedule uses the
hardware RTC only while the latest read is Ready; an unset clock, invalid clock,
or read error does not force the display off even if an older hour remains in
memory. While Power Save is On, a valid forced interval overrides Charging
keep awake: the display still turns off on external power. Leaving the interval
restores Active immediately when external power remains present.

During a forced interval the first accepted wake edge is consumed like every
other doze wake and grants a 60-second usable window. Dim may still occur at its
configured deadline, but the ordinary display-off deadline cannot shorten this
window. Each accepted input renews it. This gives enough time to enter Power
settings and disable or adjust the schedule. At the end of the window the panel
returns directly to Off. Deadline comparisons use rollover-safe unsigned time.

Keys-only Off continues to use EXT1 A/B wake plus the one-second timer that
polls PMIC power and VBUS. It remains eligible on external power when Charging
keep awake is Off or a forced interval is active. Touch + keys continues awake
polling so raw touch can wake the panel. All wake edges are consumed and require
a complete release before controls reopen.

## Persistence and compatibility

All settings use the existing `watch` Preferences namespace. Existing keys for
master sound, normal brightness, dim/off timeout, theme, indicator, button
effect, and layout remain unchanged. New keys are at most 15 characters, as
required by ESP NVS.

Wake mode now persists as byte key `wakeMode` (`0` Touch + keys, `1` Keys only).
If it is absent, the loader migrates the former boolean `buttonWake`. Saving
writes both representations so a firmware rollback retains the user's choice.
Charging keep-awake persists as `chargeAwake`. If the unpublished inverse
`chargeSave` key is present, the loader migrates its inverse; saving writes both
forms for rollback compatibility. Missing new keys receive the defaults above.
Corrupt brightness, timeout, wake, and schedule-hour values are repaired
independently.

## Validation

Focused host tests cover defaults, old-NVS migration, corrupt-value repair, all
six timeout choices, both wake modes, all new independent switches, and a full
save/load cycle. Power-policy tests cover power-save bypass, charging
keep-awake, dim-level clamping, same-day and cross-midnight schedules, equal and
invalid hours, RTC readiness, forced precedence over charging keep-awake, return
to Active after the forced interval on external power, the 60-second forced-wake
window, activity renewal, and `millis()` rollover.

On 2026-09-16, `test_power_policy`, `test_settings_timeouts`,
`test_ui_controls`, `test_watch_strings`, and
`test_settings_carousel_render` passed as independent Clang host binaries. A
full StopWatch PlatformIO integration build also passed at 50,236 bytes RAM and
1,210,729 bytes flash. Static review confirmed that the Display 6-row, Sound
4-row, and Power 9-row draw, hit, edit, save, and cancel paths use matching
indices, and that the power state transition runs before normal UI dispatch.

The tests and firmware build do not establish battery-current improvement,
AMOLED legibility at each level, physical wake reliability, speaker acoustics,
or RTC behavior under actual power interruption. Those remain device-validation
items.

## Device validation state

As of 2026-09-16, an intermediate `a0de199` image with the superseded launcher
artwork had been written successfully without erasing NVS. The final Phosphor
icon image built from `33741a5` has not yet been written. Subsequent bounded
automatic ROM-entry attempts found the same USB device node but received no
application or bootloader data, so the cause remains undetermined.

Static review does not show the power policy locking download mode. The app
sets only M5PM1 button-register bit 0 to disable single-click reset and preserves
bit 7, the download lock. Keys-only light sleep can create one-second polling
windows while the display is Off; external VBUS with Charging keep awake On
bypasses ordinary sleep, while a configured forced interval still takes
priority. With neither application telemetry nor a ROM reply available, those
conditions cannot be confirmed on the device and are not established as the
upload failure's cause.

Device validation resumes after the official manual download-mode recovery and
successful final write. Acceptance then reads `ui`, `rtc`, and `power`: the
power line must show download-lock bit 7 clear, and the UI/RTC lines must confirm
the persisted sound, wake, charge-awake, forced-sleep, screen-power, audio, and
Ready-clock states. The complete upload attempt log inventory and recovery steps
are recorded in `specs/watch-settings-carousel.md`.
