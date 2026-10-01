# Responsive controls, living motion and sound — 2026-09-06

This user goal supersedes conflicting earlier interaction notes.

- Watch: every A preset remains visibly alive; settings rows and Back/Done respond
  reliably. One pointer owner arbitrates taps/scrolling, with visible pressed
  feedback. Buttons have no implicit long-press rejection. Dragging never clicks.
- Bot: configurable Auto plus nine explicit cardinal/diagonal gaze directions;
  audit long time windows of mood/expression/animation combinations for
  frozen-looking poses. Stable faces receive appropriate motion, respecting
  explicit reduced-motion/zero settings.
  Thinking dots use native antialiasing.
- Core2: meaningful command backgrounds, pressed/held/toggle affordances,
  higher-contrast vivid themes, an independently persisted six-step sound
  volume, and contextual breathing/flow LED effects. Do not invent host toggle
  values or confirmed actions from a local key press.
- Shared UX: reusable sound synthesis, richer timbres/scales/cues and optional
  small 8-bit PCM playback. Bounded buffers, nonblocking scheduling, smooth
  envelopes, no clipping, preference-aware cancellation and selective linking.

GPT-6 medium owns shared Bot and sound work. Sol high owns Core2 controls/themes
and lighting. Parent owns Watch pointer/controls, device sound integration,
cross-review, builds, flashing and native validation. Parent exclusively owns
UxPointer.h and its test; sound agent owns the other shared sound products.

Acceptance includes focused pointer sequences (jitter, long holds, boundaries,
scroll/release, cancel/re-entry, inertia interruption), rendered pressed states,
animation time-window/AA assertions, exported sound waveform checks, both builds,
explicit-port flashing, steady timing and real Core2 reconnect. Diagnostic
canvases and synthetic pointer sequences are distinguished from physical touch
or acoustic/optical measurements. No HID user action is injected for testing.
