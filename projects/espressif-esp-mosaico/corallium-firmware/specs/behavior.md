# Corallium firmware behavior

## Factory ownership and removed functions

This is a pinned derivative of Espressif's actual factory application, retaining
its GSP renderer, touchscreen, display presenter, board manager, storage,
Wi-Fi/settings implementation, clock, battery protection and remaining apps.
Claw/AI Create, camera, music/A2DP, interaction-board, Works and Setup Center
are absent from the app registry. Startup omits Claw, ASR, HTTP configuration,
trial authentication, camera and expansion-manager services. Shared factory
components and legacy scene binding definitions remain private dependencies;
this patch does not claim to purge every inactive library/asset from flash.

The home shortcuts are Weather, Settings and IMU. Launcher has two pages; the
interaction-board page is removed. Pull-down controls are Wi-Fi, actual BLE
connection, vibration and brightness. Music player, L/R slots, unsupported
interconnect/low-power/ringtone controls have no visible or touchable route.
Hidden offscreen ancestor nodes preserve generated factory accessor ABI.

Scene bundles preserve the compiler's numeric font ordinals: `font10` follows
`font9`. Each generated GSB font reference must match the GFB at that bundle
ordinal, or resolve to the shared catalog when externalized. Build checks reject
a font that exists at the wrong ordinal and verify the final MMAP flash image
contains the validated assets. Fixed render policies use GSP's external override
table, leaving its eight inline slots for per-app capacities.

Settings root exposes Network, Display, Battery and About. Claw/AI/IM/security
integration rows are removed from the root mapping. Network's connection action
controls the BLE window instead of an inactive AP configuration portal.
About retains the locally installed firmware version, hardware identity and
protocol capabilities. Software update checking and upgrade navigation are
removed: no update page, update actions, background manifest client or official
update URL is included in this build. Firmware is installed explicitly over USB.
Weather initializes before the settings platform and Hub, so their initial
subscriptions see an initialized provider.
Battery shows charge, voltage, signed gauge current, discharge power and estimated
runtime. Missing samples immediately invalidate telemetry. Cell discharge power
is `millivolts * -average_current_mA / 1000`; charging and near-zero current are
unavailable. Runtime uses BQ27220 TimeToEmpty; 0xffff, charging or insufficient
load yields unavailable. These are gauge measurements/estimates, not USB power,
whole-system power while charging, calibrated capacity or guaranteed battery life.

## Connections

The factory USB HS console initializes through its `__wrap_app_main` hook before
application startup. `USB_HS_CONSOLE_USB_CDC_AUTO_INIT` and its required
`USB_HS_CONSOLE_USB_CDC_AUTO_DOWNLOAD` option are enabled. The board manager's
`init_skip: true` remains intentional: the wrapper owns initialization. The
vendor TinyUSB interface provides CDC diagnostics and its 303a:1001 DTR/RTS reset
sequence for returning to the ROM downloader. This console is not advertised as
a Corallium protocol transport; application commands use BLE.

[Root v1 protocol](../../../../protocols/corallium-v1/README.md) is authoritative.
BLE is discoverable only after the Bluetooth tile, Network's Bluetooth connection action or
a 500 ms top-key hold. The 120-second window expires even while connected;
the tile/network action can also close it. Holding the physical key extends it.
The broadcast name is ESP-Mosaico. Pull-down text distinguishes Bluetooth off,
Discoverable and Connected. Corallium branding appears only in About's protocol
version/capability rows, not routine hardware controls. RX and notification subscription require encrypted GATT,
secure-connections Just Works has no MITM identity guarantee. No persistent bond
is required. Service and characteristic UUIDs exactly match the root contract.

Firmware limits JSONL frames to 2048 bytes, accepts fragmented UTF-8, discards
oversized/NUL/expired partial lines through the next delimiter, serializes output
in 20-byte notifications, and clears framing/queued requests on disconnection.
A lexical guard limits JSON nesting to 16 and rejects malformed UTF-8 before the
recursive parser. Requests validate protocol version, request-only envelope,
ASCII ID, required types, duplicate top-level/payload keys, empty read/forget
payloads, integer timestamp/offset and Wi-Fi byte lengths before mutation. Passwords
are never returned or printed. One request may be in flight; queue overrun closes
the connection rather than losing a mutation response. Status notifications every
five seconds stop with the connection window.

The Wi-Fi manager is STA-only; BLE replaces automatic soft-AP fallback.
No stored SSID starts with the radio off. Network settings or wifi.set can enable
it. BLE changes use the same persistent Wi-Fi enable switch as the local UI, so
a reboot preserves the choice. wifi.forget clears stored credentials and disables
STA; time is unchanged.
Wi-Fi save success means an asynchronous join was accepted, not connected.

## Time

IDF RTC + high-resolution timers maintain local wall time after synchronization.
RTC memory validates continuity only on supported powered reset paths. Power-on
and brownout invalidate it. Startup displays --:-- when invalid; no build timestamp
or stale NVS checkpoint is presented as current. App sync works without internet.
NTP refreshes after network connection and at six-hour intervals. UTC offset is
persisted independently; it is a fixed offset, not a daylight-saving rules database.
RTC-derived time reports `source=rtc`, `quality=estimated`; app/NTP sync reports
`synchronized`. Hardware retention and long-term drift remain unmeasured.

## Power and rendering

The unused amplifier is held off, and camera/Claw/ASR/external-module background
work is absent. GPIO60 stays on because its rail is shared with the display.
Wi-Fi modem sleep is used after association. DFS permits 80–320 MHz so render work
can retain peak frequency. Automatic light sleep is deliberately disabled until
USB and touch wake behavior have been measured; factory screen timeout still
pauses the renderer and switches off the panel through its existing fence.

GSP active tick is 16 ms, pointer poll 8 ms, idle poll 50 ms, and transition snapshots
are enabled. These are scheduling targets, not measured FPS. Hub dispatcher polling
relaxes from 16 to 50 ms after two seconds without commands; foreground applications
retain 16 ms ticks. Clock hands update each second, but unchanged minute/date text
is not rerasterized every second. This reduces stable clock text update batches
from 60 to 1 per minute. Removed controls also reduce visible scene work.

Optional `CONFIG_CORALLIUM_PERF_LOG` records native rendered-frame counts and
busy microseconds every five seconds. Use the same gestures, brightness, power
source and logging settings for before/after comparisons. Idle frame rate must be
reported separately from interactive frame rate; fewer idle frames are desirable.
