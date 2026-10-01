# StopWatch idle power experience

2026-09-10 implementation contract for display power behavior in the StopWatch
companion app. This policy replaces the earlier manual Long-B dim doze.

## Power-source policy

External input power keeps the watch awake at the configured display brightness.
The app identifies external power from a valid VBUS/VIN reading above 4,000 mV,
independently of whether the battery is actively charging. A full battery can stop
charging while USB power remains connected, so the charge-status signal is not an
always-on signal.

Connecting external power wakes a dimmed or sleeping display without activating a
control. Idle dim and display-off deadlines remain disabled while external power is
present. Disconnecting external power starts a fresh battery idle interval rather
than applying deadlines accumulated while plugged in.

## Battery idle states

On battery power, activity resets two independent idle deadlines. The dim deadline
changes the AMOLED to the lowest user-selectable brightness, level 1/5 (raw value 32).
The display-off deadline puts
the AMOLED panel into Sleep In. Both deadlines are measured from the same most recent
activity, rather than making display-off an additional delay after dimming.

Each deadline has the same six choices: 5 seconds, 15 seconds, 1 minute, 5 minutes,
10 minutes and 15 minutes. The first-boot defaults are 15 seconds to dim and 1 minute
to display off. Both selections persist independently in NVS. If display-off is set
earlier than dim, the panel goes directly from awake to off; the later dim deadline
has no visible effect.

The persisted Wake setting has two modes. `Touch + keys`, the default, accepts touch,
A, B and the power/home button as wake inputs. `Keys only` accepts A, B and the
power/home button but deliberately ignores touch while the display is off. In either
mode, the wake edge is consumed: it restores the configured brightness and cannot also
select a mood, scroll, activate the underlying control, enter settings or return home.
A contact that began as a wake gesture must be released before a new touch sequence can
activate the UI. The wake itself counts as activity and restarts both deadlines.

All accepted user interactions count as activity while the display is awake. The
automatic bot animation, ambient transitions, clock redraws, serial diagnostics and
power-source sampling do not count as user activity.

## Power-save execution

The dim state retains the current screen at brightness level 1/5; on the face it
uses the existing dozing/Sleepy presentation and slower frame interval. Both dim and
display-off suspend the audio transport and power down the speaker hardware; sound is
restored only after the display returns to Active and the persisted Sound setting is on.
In the default `Touch + keys` mode, display-off keeps the ESP32-S3 awake so it can poll
the raw CST820 touch, the A/B and power/home buttons, and VBUS. In `Keys only`, the app
enters ESP32 light sleep after panel sleep and pauses both CPUs until a wake source
fires. The hardware RTC continues to keep time in both modes, and the current time is
redrawn after wake. Neither mode uses deep sleep or shutdown.

Keys-only light sleep uses EXT1 any-low wake on the direct active-low A/B inputs at ESP
GPIO2/GPIO1. Both button lines have external 10 kOhm pull-ups on the StopWatch board.
The touch interrupt and M5PM1 interrupt are intentionally absent from the wake mask.
A one-second RTC timer periodically resumes the loop to sample the latched PMIC power
button and VBUS, so the power/home button and external-power insertion can take up to
one second to wake the display. A or B wakes immediately when EXT1 observes its low
level, subject to physical-device validation of the board and button pulse.

Wake-source setup checks both ESP-IDF calls before sleeping. After every sleep attempt,
it disables the temporary EXT1 and timer sources, releases the ESP-IDF RTC GPIO holds,
returns GPIO1/GPIO2 to ordinary pulled-up digital inputs, and leaves M5PM1 IRQ routing
unchanged. A button already low immediately before sleep is returned as a wake instead
of entering level-triggered sleep. If setup or light-sleep entry fails, the ordinary
loop retries after its short awake delay rather than waiting for the one-second timer.

While the display is off, only clock continuity and the polling required to wake are
part of the user-visible experience. Bot animation, ambient scheduling, IMU motion,
sound generation, speaker hardware and display rendering remain paused. In keys-only light sleep,
FreeRTOS tasks and the normal loop are suspended between wake events. Display sleep preserves
the current face, settings page or editor, including an unsaved editor snapshot. On
wake, transient input state is cleared, the current screen is fully invalidated before
its next visible frame, and automatic ambient behavior restarts from Idle when the face
again becomes eligible.

The pinned M5GFX implementation makes this distinction concrete. On the StopWatch
CO5300 AMOLED, `M5.Display.sleep()` sets brightness to zero and sends panel Sleep In;
`M5.Display.wakeup()` sends Sleep Out and restores the display object's cached
brightness. The AMOLED `powerSave()` hook is empty. Display sleep does not call the
touch driver's sleep method, and the repository's patched CST820 sleep/wake hooks are
no-ops, so raw contact polling remains available.

Manual ESP32-S3 light sleep also suspends USB Serial/JTAG between the one-second timer
wakes. External power resumes the app at the next timer poll, but host USB reconnection
after wake remains a physical-device validation item.

## Validation boundary

Host policy tests must cover both default values, all six choices, independent
deadlines, deadline ordering, unsigned `millis()` rollover, activity resets, consumed
wake input, external-power bypass, plug-in wake and a fresh battery interval after
unplug. They must also cover both wake modes and the keys-only light-sleep decision.
Settings/string tests must cover all three persisted fields and their English and
Chinese labels and values.

A successful PlatformIO build establishes that the policy integrates with the pinned
M5Unified/M5GFX dependencies and the StopWatch firmware target. These checks do not
measure battery current, AMOLED minimum-brightness legibility, touch wake reliability,
PMIC/GPIO light-sleep wake, USB reconnection or plug/unplug behavior on physical
hardware. The keys-only mode structurally removes continuous CPU polling, but no battery
life improvement is claimed without current and duration measurements. No serial port
or connected device alone constitutes hardware acceptance.

## Final validation result

On 2026-09-10, `sh tools/check_host.sh` completed successfully. This includes the
calendar, input, timed-state, power-policy, watch interaction, controls, localization,
typography, settings-timeout, generated-mask, Core2, UX-component, pointer and sound
host tests, together with the native BotUx preview render checks. `git diff --check`
also completed successfully.

The final StopWatch PlatformIO build reported 49,868 bytes of RAM (15.2%) and
1,090,917 bytes of flash (16.6%). Its ignored
`.pio/build/m5stack-stopwatch/firmware.bin` artifact is 1,091,328 bytes with SHA-256
`5a8985cbc7846e84b959c589b90e7b64068e7060010a3f7f8b23609dd5791325`.
No firmware was uploaded for this validation.

## Dim-level correction validation

On 2026-09-11, a follow-up corrected the dim brightness unit. The runtime had passed
the value `1` directly to the raw 0–255 display API, which made the AMOLED appear off.
Both dim paths now apply user brightness level 1/5 through `Power::applyLevel()`, mapping
to raw value 32. Display-off still uses AMOLED panel sleep, and all timeout and wake
behavior remains unchanged.

The focused power-policy and settings-timeout host tests passed, followed by
`git diff --check`. The StopWatch PlatformIO build then succeeded with 49,868 bytes of
RAM (15.2%) and 1,090,893 bytes of flash (16.6%). The ignored `firmware.bin` is
1,091,296 bytes with SHA-256
`5c26e0d3a1fdd0ab324d4f946f5f7b351fb8c064a4ea81e23ab78bab11dada37`.
No firmware was uploaded.

## Authorized device deployment

On 2026-09-11, the user authorized deploying commit `0feed26` to the identified
StopWatch at `/dev/cu.usbmodem214201` (USB VID:PID `303A:1001`, serial and ESP32-S3
MAC `28:84:85:44:5B:8C`). Before upload, the ignored `firmware.bin` matched the
dim-level validation artifact: 1,091,296 bytes with SHA-256
`5c26e0d3a1fdd0ab324d4f946f5f7b351fb8c064a4ea81e23ab78bab11dada37`.

PlatformIO uploaded the `m5stack-stopwatch` environment through that explicit port
without erasing NVS. Esptool identified an ESP32-S3 QFN56 revision v0.2 with that MAC,
verified every written region, and reset the board. A bounded serial capture then saw
the normal ESP-ROM boot, the `bot-ux-watch ready` line with IMU available, and a
read-only `ui` reply on the home screen with no held keys. It contained no panic,
exception, watchdog, brownout or application error, and the serial port was closed
after capture. Logs are retained under ignored `tmp/watch-dim-deploy/`.

This deployment evidence establishes artifact identity, successful flash, boot and
read-only application response. It does not establish physical AMOLED dim-level
legibility, timeout timing, current draw or button/touch wake behavior.

## Audio sleep correction

On 2026-09-11, a user report described repeated clicks or pops while the watch was
sleeping. Code inspection found a plausible periodic mechanism in Keys-only mode: the
one-second light-sleep timer stopped and restarted ESP32-S3 digital-peripheral clocks
while the external ES8311 codec and speaker amplifier remained powered. This mechanism
has not been confirmed by acoustic or electrical measurement.

Dim and display-off now request a coordinated audio suspension. The sound source first
finishes its short release, the transport retires its own buffers and the M5Unified
speaker queue, and only the transport task that owns speaker calls powers down the
speaker. Keys-only light sleep is allowed only after the transport reports a completed
hardware suspension; a failure keeps the ordinary loop awake. One-second timer wakes
do not restore audio. A true transition to Active restores it only when Sound is on.
Persisted Sound-off startup binds the transport in its inactive state without starting
speaker hardware, and muting while Active follows the same suspension path.

Read-only serial telemetry exposes the desired audio state, transport binding,
lifecycle state, ready, suspended and failed flags, plus the StopWatch audio-power and
amplifier-enable pin levels and an I/O-read validity flag. This permits bounded device
confirmation without changing sound or power settings; it does not by itself establish
that the reported physical noise is absent.

The focused power-policy, sound and sender-lifecycle host tests passed. The StopWatch
PlatformIO build then succeeded with 50,140 bytes of RAM (15.3%) and 1,093,193 bytes
of flash (16.7%). The ignored `firmware.bin` is 1,093,600 bytes with SHA-256
`339910f688b0f370442882d8c6b755cc147bf67fd8d76a498af3ed77bf3e6139`.
No firmware was uploaded for this pre-deployment validation.

### Audio correction device deployment

On 2026-09-11, the user authorized uploading commit `7e26bcb` and that exact artifact
to the identified StopWatch at `/dev/cu.usbmodem214201`, without erasing NVS. Esptool
identified an ESP32-S3 QFN56 revision v0.2 with MAC `28:84:85:44:5B:8C`, verified
every written region and reset the board. The bounded serial capture saw a normal boot
and the application-ready line, with no panic, exception, watchdog or brownout.

The saved setting was Sound off. Read-only telemetry reported a bound, suspended
transport with valid M5IOE1 reads and both audio power and amplifier enable low. An
unsaved Display-editor replay temporarily selected Sound on; the transport reached
Ready and both lines read high. The replay then cancelled the editor through Home,
restoring Sound off, the suspended acknowledgement, both lines low and the production
face. It never selected Done or invoked the NVS save path. An initial probe used the
wrong host-side Display editor enum in its assertion and immediately recovered through
Home; the corrected bounded replay passed. The serial port was closed after capture,
and ignored logs are under `tmp/watch-sleep-audio-deploy/`.

This confirms the deployed firmware's start, stop and restore hardware lifecycle. USB
power kept the screen Active during the probe, so it does not verify battery-mode dim,
the one-second light-sleep cycle or the physical absence of the reported sound.

## Hardware and platform references

The [M5Stack StopWatch product documentation](https://docs.m5stack.com/en/core/StopWatch)
defines the ESP32-S3R8-based board, display, touch, controls, power hardware and links
the official schematics used for the pin review. The
[ESP-IDF ESP32-S3 sleep-mode documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html)
defines light-sleep state retention, timer and EXT1 any-low wake behavior, supported RTC
GPIOs, and the required RTC-GPIO release/reconfiguration after waking.
