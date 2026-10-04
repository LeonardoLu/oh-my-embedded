# Corallium firmware behavior

## Factory ownership and removed functions

This is a pinned derivative of Espressif's actual factory application, retaining
its GSP renderer, touchscreen, display presenter, board manager, storage,
Wi-Fi/settings implementation, clock, battery protection and remaining apps.
Claw/AI Create, camera, music/A2DP, interaction-board and Setup Center
are absent from the app registry. Startup omits Claw, ASR, HTTP configuration,
trial authentication, camera and expansion-manager services. Shared factory
components and legacy scene binding definitions remain private dependencies;
this patch does not claim to purge every inactive library/asset from flash.

Home opens Weather when its weather card is tapped. The card's analog clock is
replaced by Weather's exact six original dot illustrations, with the same
240×240 image scale and shared single-layer selection. Combined precipitation
symbols select one illustration; night symbols use the factory art's daytime
fallback. Missing data shows overcast art with unavailable text, and stale data
keeps its last illustration. Weather refreshes hourly and is marked stale after
six hours, so the picture does not imply a live sensor.
Settings, Works and Album shortcuts occupy the first page. The adjacent launcher
retains Settings, IMU, Album, Bricks, Weather and Works. The interaction-board
page is removed.

The pull-down has an opaque 480×480 background independent of its internal
components. Buttons form a 2×2 block: Wi-Fi/Bluetooth, then mute/vibration.
Both widget backgrounds are 220×220 and align at the same top edge.
The button block occupies less than one quarter of the screen. Buttons
contain centered icons; vertical volume and brightness sliders use embedded
icons and percentage values beside the block, with no explanatory text labels.
Music player, L/R slots and unsupported
interconnect/low-power controls have no visible or touchable route.
Hidden offscreen ancestor nodes preserve generated factory accessor ABI.
Small drawer glyphs use uncompressed RGB565+A8 resources, preserving their
transparent padding while bypassing the compressed image decoder. Button
glyphs are white in both states; slider glyphs retain the factory orange.

Scene bundles preserve the compiler's numeric font ordinals: `font10` follows
`font9`. Each generated GSB font reference must match the GFB at that bundle
ordinal, or resolve to the shared catalog when externalized. Build checks reject
a font that exists at the wrong ordinal and verify the final MMAP flash image
contains the validated assets. Bitmap and SVG refs must also resolve to the
bundle's shared GRB bank; Settings bundles the main scene's complete bank to
include its authored SVG variants. Fixed render policies use GSP's external override
table, leaving its eight inline slots for per-app capacities.

Settings root exposes Network, Bluetooth, Display, Battery, Corallium and About.
Claw/AI/IM/security integration rows are removed from the root mapping. Network's
Wi-Fi password page has no Bluetooth action; the persistent BLE switch has its
own Bluetooth page. Checked switches use the same orange track, including when
the saved value is applied after creation. Root icons use padded RGBA tint masks,
including a distinct Corallium protocol icon. Their local placeholder retains
alpha in the compiled template, so runtime uploads do not acquire an opaque
rectangle. Display selectors use a continuous, padded SVG chevron and a separate
chooser within a scrollable page, keeping stroke endpoints inside the image.
Weather's detail page separates its header condition text from the main artwork
and keeps the artwork's visible extent above the forecast cards.
About retains the locally installed firmware version and hardware identity;
Corallium opens its own protocol version, channel and capability detail view.
Software update checking and upgrade navigation are removed: no update page,
update actions, background manifest client or official update URL is included
in this build. Firmware is installed explicitly over USB.
Battery shows charge, voltage, signed gauge current, discharge power and estimated
runtime. Compact labels `Disch. Pwr` and `Runtime` fit the detail columns; values
retain mW/min units and a runtime estimate prefix. `Charging` and `--` represent
unavailable discharge measurements. Missing samples immediately invalidate
telemetry. Cell discharge power is `millivolts * -average_current_mA / 1000`;
charging and near-zero current are unavailable. Runtime uses BQ27220 TimeToEmpty;
0xffff, charging or insufficient
load yields unavailable. These are gauge measurements/estimates, not USB power,
whole-system power while charging, calibrated capacity or guaranteed battery life.

## Startup and local Works

Saved Wi-Fi/Bluetooth enable choices, brightness, rotation, volume/mute,
vibration and idle policy are restored before the native UI begins. Weather
provider/cache initialization and provisioning callback setup happen locally
before UI subscriptions. They do not start radio association or HTTP requests.
Wi-Fi association and the weather worker are queued after UI startup; the first
page and control center do not wait for either result. Unavailable data has
explicit placeholder states.

Prepared Wi-Fi startup consumes the worker's current desired configuration,
preserving commands queued by an early local or BLE interaction. Ordinary
configuration commands own copies of all strings and rebind pointers after
queue copying. Startup therefore neither restores a stale pre-UI enable choice
nor refers to a caller's expired credential storage.

Works is restored with Lab, Recent and Installed views. Lab contains the factory
Dino and Flappy Bird games. Installed enumerates the local app catalog; Recent records launched apps in local storage. The runtime
and required Lua capabilities initialize lazily on first opening Works, without
starting Claw/AI, cloud authentication or an HTTP configuration portal.
The local bootstrap also initializes the empty tagged-lease registry used by
Lua job cleanup; it does not start a hardware bridge or acquire device leases.

The experimental Fluid, Dot Fluid and Liquid Toy apps are removed: device
feedback found their motion too slow and their appearance unsuitable. Their
private solvers, Lua modules, gravity provider and app assets are not shipped.

## Connections

The factory USB HS console initializes through its `__wrap_app_main` hook before
application startup. `USB_HS_CONSOLE_USB_CDC_AUTO_INIT` and its required
`USB_HS_CONSOLE_USB_CDC_AUTO_DOWNLOAD` option are enabled. The board manager's
`init_skip: true` remains intentional: the wrapper owns initialization. The
vendor TinyUSB interface provides CDC diagnostics and its 303a:1001 DTR/RTS reset
sequence for returning to the ROM downloader. This console is not advertised as
a Corallium protocol transport; application commands use BLE.

[Root v1 protocol](../../../../protocols/corallium-v1/README.md) is authoritative.
BLE defaults off. Settings → Bluetooth, the Bluetooth tile or a 500 ms top-key hold
changes the switch; successful changes persist before runtime state changes.
Startup restores the saved switch, factory reset clears it, and there is no
connection deadline. Disconnection resumes advertising while enabled. The pull-down Bluetooth tile
is charcoal while off and orange while enabled, with a white glyph in both states.
A non-interactive lower-right `1` badge appears only while its single supported
peer is connected; disconnect hides the badge without changing the enabled
color. The broadcast name is ESP-Mosaico. Corallium branding belongs to the
independent protocol settings entry/detail, not routine controls. RX and
notification subscription use ordinary
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
USB and touch wake behavior have been measured.

Display persists dim, screen-off and whole-device power-off delays in NVS, along
with independent dim/screen-sleep switches for charging. Invalid stored timeout
values fall back to defaults; factory reset restores those defaults. Choices
are measured from the same last screen activity:

| Setting | Choices | Default |
| --- | --- | --- |
| Dim after | 5, 10, 15, 30 seconds; 1 minute; Never | 10 seconds |
| Screen-off | 10, 30 seconds; 1, 2, 5 minutes; Never | 30 seconds |
| Power-off | 5, 10, 15, 30 minutes; 1 hour; Never | Never |
| Dim screen while charging | On/off | Off |
| Sleep screen while charging | On/off | Off |

Dimming caps hardware brightness at 15% without increasing an already lower
setting. It keeps runtime brightness separate from the saved user level,
including an unsaved slider preview; activity or wake restores that user level.
Screen-off flushes and pauses the GSP renderer/panel through the existing
presenter fence and suspends ordinary child-app timers. Touch/button wake
resumes rendering; the first physical wake touch is consumed until release.
The shutdown timer continues after screen-off. A charging/availability change
starts a fresh inactivity interval.

Whole-device power-off applies only with available gauge data reporting no
charging. The callback checks that condition again before a 100 ms active-low
GPIO57 pulse; it never switches GPIO60, whose rail is shared with the display. Charging here is
the BQ27220 net-cell charging state, confirmed over three five-second samples,
not a USB cable-presence measurement. A connected supply supporting a full or
discharging cell may therefore report no charging. Missing gauge samples block
auto power-off. The default Never avoids inferring external-power presence.

Foreign RAW/LVGL presenter ownership defers GSP dimming, panel-off and shutdown.
Dino/Flappy RAW jobs and Album's LVGL presenter do not cooperate with idle
deadlines; their idle effects apply after returning to GSP. The firmware does
not pause the panel underneath a foreign owner.

GSP active tick is 16 ms, pointer poll 8 ms, idle poll 50 ms, and transition snapshots
are enabled. These are scheduling targets, not measured FPS. Hub dispatcher polling
relaxes from 16 to 50 ms after two seconds without commands; foreground applications
retain 16 ms ticks. Unchanged status/AOD minute/date text is cached rather than
rerasterized each second, reducing stable text update batches from 60 to 1 per
minute. Home reuses Weather's static image assets and changes visibility only
when applying a staged snapshot. There is no independent weather drawing,
Canvas producer, borrowed frame pair or precipitation timer. The complete
painted masks fit the Home card and remain clear of the text. Removed controls
also reduce visible scene work.

Optional `CONFIG_CORALLIUM_PERF_LOG` records native rendered-frame counts and
busy microseconds every five seconds. Use the same gestures, brightness, power
source and logging settings for before/after comparisons. Idle frame rate must be
reported separately from interactive frame rate; fewer idle frames are desirable.
