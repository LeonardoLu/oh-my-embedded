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

Home displays clock/weather information and opens Weather when its weather card
is tapped. Weather, Settings and IMU shortcuts sit along the bottom. The adjacent
launcher retains Settings, IMU, Album, Bricks and Weather. The interaction-board
page is removed. Pull-down buttons form two columns: Wi-Fi/Bluetooth, then
mute/vibration. Buttons contain only centered icons; volume and brightness
sliders and their values sit below. Music player, L/R slots and unsupported interconnect/low-power controls
have no visible or touchable route.
Hidden offscreen ancestor nodes preserve generated factory accessor ABI.

Scene bundles preserve the compiler's numeric font ordinals: `font10` follows
`font9`. Each generated GSB font reference must match the GFB at that bundle
ordinal, or resolve to the shared catalog when externalized. Build checks reject
a font that exists at the wrong ordinal and verify the final MMAP flash image
contains the validated assets. Fixed render policies use GSP's external override
table, leaving its eight inline slots for per-app capacities.

Settings root exposes Network, Display, Battery, Corallium and About. Claw/AI/IM/security
integration rows are removed from the root mapping. Network's connection action
controls the persistent BLE switch instead of an inactive AP configuration portal.
About retains the locally installed firmware version and hardware identity;
Corallium opens its own protocol version, channel and capability detail view. Software update checking and upgrade navigation are
removed: no update page, update actions, background manifest client or official
update URL is included in this build. Firmware is installed explicitly over USB.
Weather initializes before the settings platform and Hub, so their initial
subscriptions see an initialized provider.
Battery shows charge, voltage, signed gauge current, discharge power and estimated
runtime. Compact labels `Disch. Pwr` and `Runtime` fit the detail columns; values
retain mW/min units and a runtime estimate prefix. `Charging` and `--` represent
unavailable discharge measurements. Missing samples immediately invalidate telemetry. Cell discharge power
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
BLE defaults off. The Bluetooth tile, Network action or a 500 ms top-key hold
changes the switch; successful changes persist before runtime state changes.
Startup restores the saved switch, factory reset clears it, and there is no
connection deadline. Disconnection resumes advertising while enabled. The pull-down Bluetooth icon stays grey while off and orange while enabled.
A non-interactive lower-right `1` badge appears only while its single supported
peer is connected; disconnect hides the badge without changing the enabled color. The broadcast name
is ESP-Mosaico. Corallium branding belongs to the independent protocol settings
entry/detail, not routine controls. RX and notification subscription use ordinary
GATT Write/Read permissions without pairing or encryption. Firmware does not
initiate pairing and explicitly rejects security requests; no authentication
completion callback gates the connection. JSONL, including Wi-Fi credentials,
travels in plaintext with no application-layer encryption. No persistent bond is
created or required. Local BLE on/off and factory-reset switch handling do not
manage Bluetooth keys. UUIDs exactly match the root contract; StopWatch retains
its separate encrypted profile.

Firmware limits JSONL frames to 2048 bytes, accepts fragmented UTF-8, discards
oversized/NUL/expired partial lines through the next delimiter, serializes output
in 20-byte notifications, and clears framing/queued requests on disconnection.
A lexical guard limits JSON nesting to 16 and rejects malformed UTF-8 before the
recursive parser. Requests validate protocol version, request-only envelope,
ASCII ID, required types, duplicate top-level/payload keys, empty read/forget
payloads, integer timestamp/offset and Wi-Fi byte lengths before mutation. Passwords
are never returned or printed. One request may be in flight; queue overrun closes
the connection rather than losing a mutation response. Status notifications every
five seconds require an enabled, connected, subscribed session and begin only
after the session's device.info response has completed.

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

The output-only speaker service uses the official ES8311 board DAC/PA path.
Only a local volume release or unmute requests a 100 ms confirmation; startup
and background activity are silent. Codec volume and mute apply to actual PCM
output. After the tone, a finite silent tail advances the actual I2S DMA ring
capacity so queued samples play before muting; failed writes/open or local mute
close output without retrying indefinitely. The DAC, I2S output and PA close
after feedback. No audio capture or continuous mixer task starts. Volume zero
persists mute; the previous nonzero volume is stored separately for unmute across
restart. Save errors are surfaced
and the UI refreshes the actual state. Camera/Claw/ASR/external-module background
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
