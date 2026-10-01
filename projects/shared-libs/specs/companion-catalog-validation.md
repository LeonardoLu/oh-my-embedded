# Companion controls, feedback and catalog acceptance

2026-09-06. Current contracts are [Watch settings](../../m5stack-stopwatch/bot-ux-watch/specs/watch-settings-iteration.md),
[Bot vocabulary](../bot-ux/specs/bot-vocabulary-iteration.md), and
[Core2 Alive lighting](../../m5stack-core2/bot-ux-codex-core2/specs/core2-alive-lighting.md). Earlier validation records remain
historical and are not retroactively assigned the new enumeration counts.

## Shared Bot and catalog

Bot commits `a777f1f` and `a2c54b9` preserve prior enum values and append Asleep
as mood 12. The current API exposes 13 moods, 10 expressions and 8 animations:
1,040 combinations. Waiting has patient open eyes, Asleep has closed eyes and
fading vector z marks, Thinking dots have larger travel, and all ordinary faces
share Idle's placement. Nine explicit gaze directions plus Auto remain available;
downward travel is closer to center. Sleeping marks alpha-composite over actual
underlying pixels across all body styles, while normal opaque eyes retain their
span renderer.

The focused native renderer passes its full suite, including sustained movement
across all 1,040 combinations, nine-direction morphology and mirror checks,
RGB565 edge coverage, sparse/eased updates, and reduced/zero motion. Thinking
center-dot travel at 200px is 68.5 / 13.5 / 0px full/reduced/zero. All four body
styles pass two-cycle Asleep wrap tests; Round maximum wrap/ordinary adjacent
frame differences are 1,041 / 4,172 full and 207 / 828 reduced, with zero amount
identical. Details and the primitive-raster limits are in the Bot record.

`wiki/bot-ux/intro.md` and self-contained `intro.html` cover all 31 enumeration
entries, including both Auto selectors. Bilingual explanations accompany 72
native C++ RGB565 frames per entry at 120×120 and 10fps. The 31 GIF assets are
stored beside Markdown and embedded inside HTML. A static nine-direction appendix
is regenerated from the current native renderer, including the closer downward
pose. Existing `projects/shared-libs/bot-ux/docs/nine-directions-raster.png` is refreshed with it.

CUA browser verification, independently accepted by the root reviewer, confirms
Chinese search `熟睡` returns 1/31, the Animation category returns 8/31, pause and
play switch static PNG/live GIF, and the full view contains 31 entries. Desktop
and 390×844 screenshots were inspected; mobile document width is 390px and card
width 358px, without horizontal overflow. Images display at their native 120px
size. The HTML embeds its assets and needs no network; localhost port 8765 is
only a convenient review server. GIF color quantization and the finite sampling
loop are not evidence of the live firmware's timing or loop continuity.

The generated Cjk18 subset includes the new `熟睡` / `正在熟睡` firmware text:
340 glyphs and 43,259 coverage bytes. Documentation-only Chinese catalog copy is
not added to the bounded firmware corpus.

## Watch verification

Watch commits `678483b`, `d43e040` and diagnostic follow-up `a926d87` are documented
in `projects/m5stack-stopwatch/bot-ux-watch/specs/watch-settings-iteration.md`. Read-only cross-review confirmed shared
row geometry for drawing/hit tests, four full main-list slots and two preview
slots, viewport-clipped rounded release targets, one centered Done, and touch
clearing persistent button selection. A/B use the complete current mood set;
double B restores automatic Idle. The stationary face hold is three seconds.

Cross-review identified and the owner fixed two integration issues before final
acceptance: gaze had changed saved animation to Auto without restoring it, and
the shared Bot battery override still closed manually selected eyes at low
battery. `presentation()` now prioritizes temporary gaze, then manual selection,
then saved expression/animation. The HUD retains the real percentage while manual
mood/gaze temporarily suppresses only BotUx's low-battery eye override. The helper
has focused restoration/Speaking tests; the fix was independently re-read.

The full root host runner passes 21 reported stages (20 standalone contracts and
the real BotUx renderer), including current Asleep alpha checks. The final Watch
upload log reports RAM 37,736 B, flash 726,149 B and success on explicit port
`/dev/cu.usbmodem214201`, with verified flash hash and reset. The final source includes `a926d87`.

Native injected-contact verification passed, with eight key captures inspected
by root: Settings, Bot Personality, pressed Done, Motion top and scrolled bottom,
Combinations bottom, Working temporarily centered to Front, and Speaking looking
toward an outside contact. Motion reached offset 120 and Combinations offset 60,
revealing the last fields without colliding with Done. A 2.15-second stationary
hold stayed on Face; a 3.15-second hold opened Bot Personality. Diagnostics showed
center touch temporarily using Idle (0), then restoring Working (8); outside
touch temporarily using Idle (0), then restoring Speaking (3). Appended Asleep
(12) rendered successfully. These exercise the actual firmware canvas and input
path; contacts remain diagnostic injections rather than physical finger tests.

The owner then restored the initial runtime state by hardware reset without
writing NVS: Face, manual selection off, mood/effective mood Idle, language 1,
and original saved preferences preserved. The serial port was released. Final
Watch evidence is committed as `f695d3a`.

## Core2 verification

Core commits `97218ad` and `bcb917b` cover feedback implementation and final
integration evidence. A fresh Working → NewReply event starts a 30-second local
attention window. Persistent green cannot start or renew it; user interaction,
new host state, stale/disconnected feedback and expiry clear it. Host colors and
states remain authoritative after the attention ends. The independent C++11
Wall/Wextra/Werror review run of `fresh_reply_attention_test` passed. Review of
main-loop and LED integration found no remaining substantive defect.

The owner's focused contracts pass for the attention timer, LED output and
feedback levels. Final firmware, including the last shared Bot/CJK changes,
builds at RAM 60,424 B and flash 1,507,629 B. Upload to explicit port
`/dev/cu.usbserial-5C9A0591461` wrote 1,514,208 bytes, verified the hash and reset
cleanly. After reset, BLE restored app-ready MTU 67, Bottom2 initialized GPIO25,
and the diagnostic sample reported speaker master 128, synth gain 255, audio
on, idle sender and zero failures. Steady rendering was approximately 29.4 FPS.

Native six-step volume checks observed gains 0,16,30,45,96,255; nonzero changes
started a preview after the new gain applied, while zero and mute stayed idle.
Eight maximum Confirm cues completed without sender failures. General, Paper,
Warm and Dark 320×240 native canvases were inspected by the owner and root for
readability, surface separation and accent contrast. No HID action or fabricated
RPC state was injected. See the Core record for software speaker headroom and
all distinctions between local animation and authoritative host feedback.

## Evidence boundaries

Firmware uploads and app-ready diagnostics prove compilation, flash transport
and the observed runtime link state. Native captures are framebuffer output,
not panel photographs; diagnostic contacts bypass the physical touch IC. Native
sound diagnostics prove bounded sender operation, not acoustic loudness or
distortion. LED color calculations are not optical measurements. This iteration
does not infer physical acceptance from connected ports alone, and does not
claim that host green means confirmed task completion.
