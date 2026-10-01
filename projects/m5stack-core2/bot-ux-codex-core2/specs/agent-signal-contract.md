# Core2 agent lighting signal projection

`projects/m5stack-core2/bot-ux-codex-core2/include/AgentSignal.h` is a pure C++ helper. It projects
signals emitted by the locally inspected Codex app build, rather than claiming
a complete task-state protocol. It does not own BLE, the display, sound, or BotUx.

## Verified mapping

| Exact RGB24 color | Official status | `agentsignal::Signal` | UI text |
| --- | --- | --- | --- |
| `0x304FFE` | working | Working | Working |
| `0xFF6D00` | awaiting-approval or awaiting-response | NeedsInput | Needs input |
| `0x00FF4C` | unread | NewReply | New reply |
| `0xFF0033` | error | Error | Error |
| `0xFFFFFF` | idle | Idle | Idle |
| effect off, or zero color with solid/breath | lights off | Off | Lights off |
| no app-ready transport / unknown color or effect | unknown | Unknown | Unknown |

Decode RGB24 before any RGB565 conversion; never use nearest-color guessing.
The inspected per-slot writer emits solid (1) or breath (4). Other nonzero effects
are Unknown, even if their base color resembles the status palette. Brightness
is a user preference: brightness zero alone does not erase a known status color
with solid/breath effect. Off describes lighting, not task assignment or completion.

## Evidence in the inspected official app bundle

The bundle copies are research artifacts under `tmp/codex-hid-research/`; the
semantics and limitations below are retained here so that scratch files are not
the sole implementation contract. Offsets disambiguate minified one-line files.

- `webview/assets/codex-micro-slot-signals-1d809a417a41.js:1`, function `Oe`,
  character offset 3252: local error wins; then approval/response pending chips;
  then loading maps to working; then unread; otherwise idle. Remote failed maps
  to error, pending/in_progress to working, then unread, otherwise idle.
- `webview/assets/app-initial-86767c3d23e5.js:1794`, `Vvt`, offset 1998331:
  defines the exact color table above. Both pending-input kinds share orange.
- `service-readable.js:1`, `$`, offset 28062: emits `status` color per slot,
  selected or pulsing gets breath, otherwise solid. **Breathing is not working.**
- `node_modules/@worklouder/device-kit-oai/dist/rpc_api_oai/rpc_api_oai.js:139`:
  `sendThreadsLighting` serializes only id/c/b/e/s/sk/sa into `v.oai.thstatus`.
  No task identifier, title, semantic status string, completion event, or host
  revision is carried. Optional fields leave existing device fields unchanged;
  the caller therefore supplies the merged LightingState snapshot to the model.

## Information that the wire does not preserve

There is no Done state. Green means unread, not confirmed successful completion.
The slot-signals `Te` function turns selected+focused unread into idle. Idle can
also represent a custom command slot without a task. Approval and response cannot
be distinguished. Slot numbers 1–6 are physical assignments, not task IDs.

`service-readable.js:1` `applyInactivityLightingOff` sends six off slots when the
lighting timeout expires. All-off does not establish unassignment or task ending.
The host setting schema names this preference `codex-micro-lighting-auto-off`,
offers `off`, 30 seconds, 1/3/10/30 minutes and 1 hour, and defaults to 3 minutes.
The public Codex Micro settings screen labels it **Auto-dim**; choosing **Off**
maps `inactivityTimeoutMs` to null, so the host does not schedule the all-off RPC.
Core2 has no handshake capability that can override this host preference and must
not guess that an all-off packet was auto-dim or emit fake HID activity to reset it.
The inspected bundle evidence is `app-initial-86767c3d23e5.js:1`, offsets 1998301
and 2158504 (duration mapping and persisted setting schema),
`codex-micro-settings-2a601a7f9b9a.js:1`, offset 64940 (public selector), and
`service-readable.js:1`, offset 24590 (timer and all-off RPC path).

For the September 2026 device validation, the user explicitly set the public
Auto-dim selector to **Off**. The host mapping above is the deterministic evidence
that this makes the timeout null. Periodic `device.status` requests do not call
`handleLightingActivity`; the inspected service calls it only from HID events and
joystick motion outside the dead zone (offsets 14227 and 14844). Two attempted
passive device observations were therefore treated conservatively: BLE and the
app-ready control plane stayed connected, but `sentEventCount` increased during
each window. Since that counter covers successful outbound key, encoder, and
joystick messages, neither window proves a full three minutes without device
activity. They do establish that the firmware did not lose BLE or app readiness
during the observed spans; they are not used to reinterpret any all-off snapshot.

`webview/assets/codex-micro-bridge-7749dc2a7114.js:1`, offset 12022, permits
onboarding, mini-game and composer-navigation overrides of slots. `Ft` (offset
5305) can produce an error-colored first slot for navigation. The onboarding and
mini-game modules also supply synthetic slot arrays. The transmitted fields do
not expose these override origins. Source-grounded color decoding is useful as
a display projection, but it cannot guarantee that every received color is an
actual agent transition. Do not present a success claim or run automation based
on this signal alone. A future semantic host extension would be required for
reliable identities, distinct needs-input reasons, and completion events.

## API and integration

- `decode(const LightingZone&, bool ready)` and `name(Signal)` are stateless.
  `ready` is the full Codex app handshake readiness, not BLE pairing alone.
- After reconnect, gate `ready` on a fresh thread-lighting snapshot, or clear the
  caller's cached slots on disconnect. A stale pre-disconnect LightingState
  presented as ready can otherwise consume the silent baseline before the new
  host snapshot arrives. RGB config receipt alone is not evidence of fresh slots.
- `Model::update(const LightingState&, bool ready, uint32_t nowMs)` returns a
  six-bit notification mask. Call from the main loop with a coherent snapshot.
- `Model::slot(i)` exposes `Snapshot { zone, signal, revision, changedAtMs }` for
  a valid zero-based slot index. `Model::revision()` covers the whole model.
- A slot revision increments when signal or any zone field changes. Its timestamp
  records that change; duplicate packets do not restart animations. The global
  revision increments once per update containing at least one changed slot.
  These are local display revisions, not revisions supplied by the host.
- Disconnect clears displayed snapshots to Unknown. The first app-ready snapshot
  establishes a silent baseline. All-off suspends the baseline; restoration is
  silent. Unknown/Off-to-known transitions also establish evidence silently.

For UI color interpolation, retain the previous rendered color when a new local
revision arrives. Use the snapshot's original host color for its status marker;
select contrasting text and a separate selected border/slot badge. Bot follows
only the selected slot: Working → Working/slow Orbit; NeedsInput → Waiting;
Error → Blocked; Idle → Idle/Calm; Off → neutral or Sleepy. NewReply may receive
a short happy visual acknowledgment followed by a settled pose. That visual is
not evidence of completion. Choosing another slot is not a task-state transition.

The six card backgrounds are fixed local slot-identity colors, with separate
light, warm and dark sets. A bordered dot keeps the exact host RGB status visible,
the label keeps its protocol name, and the selected outline and pressed fill are
independent interaction layers. Thus six simultaneous Working slots remain easy
to distinguish without changing or hiding the authoritative blue status signal.

## Notification behavior

A changed known active signal entering NeedsInput or Error, or Working → NewReply,
may set its slot bit once. First snapshots, reconnect snapshots, inactivity
restoration, unchanged states, and Unknown/Off transitions do not alert. Default
cooldown is 2500 ms independently per slot, configurable through the Model
constructor. A cooldown-suppressed event is dropped, not replayed later. Timing
uses unsigned elapsed subtraction so `millis()` wrap is handled normally.

The main UI can collapse simultaneous bits into one short sound and highlight
all affected cards. Follow the user's audio preference; preserve the selected
Bot even if a different slot needs attention. Wording is “Needs input”, “Error”,
or “New reply”, never “Task complete”. Native host overrides and slot reassignment
remain intrinsically indistinguishable; cooldown and baseline handling reduce
noise but do not prove event identity or remove that protocol limitation.

## Host validation

Run:

```sh
c++ -std=c++11 -Wall -Wextra -Werror \
  -Iprojects/m5stack-core2/bot-ux-codex-core2/include \
  projects/m5stack-core2/bot-ux-codex-core2/test/agent_signal_test.cpp \
  -o /tmp/agent-signal-test
/tmp/agent-signal-test
```

Tests cover exact/unknown colors and effects, brightness-zero semantics,
ready-versus-connected distinction at the API boundary, duplicate packet
revisions, effect-only presentation changes, transition notifications,
independent cooldowns, no delayed replay, reconnect and all-off restoration
baselines, unknown-to-known suppression, and time wrap. They run without Arduino,
M5GFX, BLE, sound, or display dependencies.
