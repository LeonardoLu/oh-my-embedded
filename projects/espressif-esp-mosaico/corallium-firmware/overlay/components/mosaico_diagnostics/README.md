# USB diagnostic service

`CONFIG_MOSAICO_USB_DIAGNOSTICS` enables one low-priority task after local UI
startup. `mosaico_diagnostics_start(ops)` requires the board's already initialized
USB CDC console. It reuses non-blocking `read(STDIN_FILENO)` and CDC interface 0;
it does not install USB, change the board's DTR/RTS reset callback, start Claw CLI,
or change ordinary Corallium BLE.

The callbacks expose a fixed public status structure and queued UI operations.
Status, tasks and capture must not wake the display, reset inactivity, or start
network work. Input callbacks use the existing UI/loader queue. `OK queued`
confirms gesture submission. `open settings|works|album|weather` invokes only a
fixed enum callback; `OK admitted` confirms asynchronous launch queue admission,
and subsequent input/runtime errors remain visible in status.

## Wire protocol

The host sends ASCII lines terminated by LF or CRLF:

```text
@MOSAICO 1 ping
@MOSAICO 2 status
@MOSAICO 3 tap 240 120
@MOSAICO 4 drag 240 10 240 330 400
@MOSAICO 5 back
@MOSAICO 6 capture
@MOSAICO 7 abort
@MOSAICO 8 tasks
@MOSAICO 9 open settings
```

IDs are decimal integers from 1 through 4294967295. Coordinates are 0 through
479, and drag duration is 50 through 2000 milliseconds. Lines have at most 128
bytes before the terminator. One prefixed command is accepted per 100 ms;
invalid commands also consume this interval. Extra arguments, negative numbers,
non-ASCII/control bytes and overflow are rejected. Other console input is
discarded, and input is never echoed or interpreted as a shell/CLI command.

All response lines have the prefix `@MOSAICO <id>`. Each packet starts and ends
with CRLF to resynchronize with existing log output. A malformed ID or invalid
line can receive ID 0. Ping and status report the six-byte factory eFuse MAC
using `esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY)`. ESP32-S31's default eFuse getter
produces an eight-byte EUI-64; the explicit factory type guarantees MAC-48:

```text
@MOSAICO 1 OK mac=1c:29:04:d0:90:36 protocol=1
@MOSAICO 3 OK queued
@MOSAICO 5 ERR ui 0x107
```

Error codes are fixed lowercase words: `syntax`, `argument`, `command`, `line`,
`rate`, `busy`, `unsupported`, `ui`, `status`, `asleep`, `owner`, `frame`, `timeout`,
`cancelled`, `tasks` and `overflow`. Provider failures append the numeric ESP error in
hexadecimal. No request text or configuration payload appears in an error.

Status uses four bounded JSON lines with the same ID. Merge their disjoint top
level objects until the final `OK`:

- `STATUS {"device":{"chip":"esp32s31","mac":"..."},"uptime_ms":...}`
- `STATUS {"ui":{...}}`
- `STATUS {"render":{...}}`
- `OK {"controls":{...},"battery":{...}}`

`ui` includes the public catalog name/ID, owner (`none`, `gsp`, or `exclusive`),
dimensions/rotation, started/asleep/dimmed/panel/hub/runtime/quiesced/paused/drawer
state, gesture/queue state and capture capability. `render` includes actual
`frames`, `busy_us`, `errors`, `last_error`, `runtime_errors`,
`last_runtime_error`, and `last_input_error`. These counters do not claim a
target tick interval was achieved. `controls` includes volume, saved/user
`brightness`, actual `panel_brightness_percent`, three idle deadlines, charging
switches and Wi-Fi/Bluetooth switches. `battery` contains availability and
charging. Boolean values are JSON 0/1. No SSID, password, cloud credentials,
scene text or arbitrary strings are exposed.

## Live task statistics

`tasks` uses the enabled FreeRTOS trace/runtime-statistics public APIs. It takes
one fixed 64-slot `uxTaskGetSystemState` snapshot, then compares opaque snapshot
handles with public `xTaskGetHandle` lookups for 14 constant names. Captured
name, TCB and stack pointers are never dereferenced after the SDK returns.
Formatting and USB output occur after the SDK has released its internal kernel
lock; the command takes no UI/runtime mutex and never changes idle activity.
Capacity overflow or a concurrent task increase beyond 64 returns `ERR tasks`.

```text
@MOSAICO 8 TASKS {"snapshot_ms":9000,"total_tasks":24,"listed_tasks":14,"counter_bits":32,"counter_total_us":9000000,"pc_supported":false}
@MOSAICO 8 TASK {"name":"esp_gsp","present":true,"number":17,"state":"blocked","priority":4,"base_priority":4,"stack_min_bytes":768,"runtime_us":1093820,"affinity":-1}
...one TASK line per fixed name...
@MOSAICO 8 OK {"tasks":14}
```

Order is `esp_gsp`, `gsp_decode`, `mosaic_runtime`, `screen_power`, `works_runtime`,
`usb_diagnostics`, `esp_timer`, `Tmr Svc`, `IDLE0`, `IDLE1`, `wifi_manager`,
`weather`, `main`, `sys_evt`. Missing/deleted-between-samples tasks have
`present:false`, `state:"missing"`, zero counters/priorities and affinity -1.
Present states are `running`, `ready`, `blocked`, `suspended`, `deleted`, or
`invalid`. A snapshot and the subsequent name lookup are not one atomic sample;
short-lived tasks can disappear between them. `gsp_decode` is normally absent
when asynchronous decoding is disabled.

`stack_min_bytes` is the public lifetime stack high-water value in bytes. Runtime
values use the selected ESP timer counter; this configuration is U32 and wraps
after about 71.6 minutes. Compare modulo the reported `counter_bits`; zero values
do not imply a stopped task. `affinity` is a pinned core (0/1) or -1 for no
affinity, not the core currently executing a task. The diagnostics task itself
may report running while taking the sample.

No safe public live foreign-task PC or blocked-object getter is available for
this configuration. `pc_supported:false` makes that limit explicit; no saved
stack, return address, register dump or data address is exposed. A blocked state
alone cannot identify a queue, mutex or panel fence; combine repeated runtime
samples with the loader's pause/quiesce probes. Task snapshots require a full
second between requests, stream bounded packets, accept only `abort` during the
stream, and stop after 5 seconds total or 2 seconds without TX progress. Capture
and task streams cannot overlap. Disabled trace/statistics returns `ERR tasks`
with `ESP_ERR_NOT_SUPPORTED`.

## Real-frame transport

Capture requires an awake GSP presenter and an available capture provider.
Asleep/disabled-panel requests return `ERR asleep`; an exclusive presenter
returns `ERR owner`; unsupported capture returns `ERR unsupported`. Capture does
not wake the screen. The UI provider applies a finite budget to locking, pause
and flush, then releases UI/presenter locks before returning an immutable
snapshot handle. Successful copies resume rendering before transfer. The SDK
resume call has no caller-controlled deadline; a failed resume retains its token
for recovery by subsequent input. No USB wait holds a UI lock. The transport never produces a
host-rendered substitute.

```text
@MOSAICO 6 FRAME 480 480 960 RGB565LE 460800
@MOSAICO 6 DATA 0 192 12345678 <base64>
@MOSAICO 6 END 460800 12345678
```

`FRAME` fields are width, height, byte stride, format and total bytes. RGB565 is
little-endian; row padding is included. Width/height are at most 480, stride is
between `width * 2` and 1024, and `total == stride * height <= 491520`. `DATA`
fields are zero-based byte offset, decoded length (at most 192), eight-digit
IEEE CRC32 and Base64. `END` contains total size and CRC32 of the entire frame,
including padding. CRC32 matches standard zlib initialization/finalization.

The host must verify identity before input, reject gaps/duplicates, malformed
Base64, mismatched lengths/CRCs and missing `END`, and only save a complete
validated frame. Ordinary logs share CDC. The sender checks capacity for the
whole remaining packet before a raw CDC write, tracks short writes, and applies
finite retry deadlines. It takes no manual libc FILE lock. In the linked
Picolibc, `ftrylockfile` obtains the global libc recursive mutex while
`funlockfile` releases the independent FILE mutex; pairing them can retain the
global mutex and block other tasks. No private FILE layout or SDK change is used
to work around that mismatch. Concurrent console writers can still interleave;
the host rejects malformed/incomplete JSON or frame CRC/length failures. A failed
transfer may be retried and never displayed as a successful capture.

Only one snapshot is active. `abort` releases it and sends `ERR cancelled` under
the capture ID followed by `OK aborted` under the abort ID. Disconnect, provider
error, 2 seconds without TX progress, or the 30-second capture limit also release
the snapshot. UI copying may take longer than the USB stall deadline, so that
deadline begins when its transport starts. The 30-second limit includes copy
time and is checked when the provider returns; it cannot interrupt a blocked
SDK call. Four fixed 512-byte output slots, a 64-byte read buffer,
bounded line storage, and one 192-byte frame chunk avoid unbounded queues.

## Verification

Run from the repository root:

```sh
python3 projects/espressif-esp-mosaico/corallium-firmware/tests/check_diagnostics.py
```

The check compiles the actual parser and actual service task with ASan/UBSan and
explicit CDC/UI boundary doubles. It covers malformed input, limits, rate and
backpressure, read-only status, actual-command dispatch, partial writes,
zero manual FILE-lock calls, slow UI copy, cancellation/disconnect/timeout cleanup, and
the disabled configuration. Task tests exercise actual collector code against
explicit public RTOS boundary doubles, fixed names, unusable captured pointers,
capacity/race failures, task states, both counter widths, read-only streaming,
rate limits, cancellation/disconnect/stall/total deadlines and affinity. The C
formatter's task transcript is decoded by the real host client through split
memory reads. MAC tests use the pinned SDK header/type when
available, check the factory address's six-byte table entry, and restore the
old getter only in an ignored negative-control copy. Its eight-byte S31 write
must fail ASan. A separate negative-control copy restores the former transport
with boundary doubles matching the reviewed linked Picolibc lock behavior; its
global recursive lock stays held after three packets despite matching numbers
of FILE API calls. The corrected actual transport never calls those doubles.
This model is a software regression, not a measured device lock state.
Independent Python checks decode the maximum-sized
frame transcript, row padding, Base64 tails and CRCs. The actual host client also
decodes these C-formatted packets through split memory reads. Generated bytes
are host test fixtures, not physical framebuffer evidence; the check never opens
a serial port. Firmware linking and device transfer require separate validation.
