# Shared-library and consumer path migration — 2026-10-02

The repository is now `oh-my-embedded`. Shared libraries moved from root `lib/`
to `projects/shared-libs/`; their two consumers moved beneath brand-model device
directories. The PPS app moved alongside the Core2 controller, retaining its
private vendor library. Layout and ownership are described in
[the project index](../../README.md).

## Implementation

- Both companion `platformio.ini` files use `../../shared-libs`.
- Root host checks and source-reading font tests use the new project paths.
- Bot host-preview scripts and the catalog packager resolve the repository root
  one level higher; default scratch output remains in root `tmp/`.
- Specs, screenshots and profile JSON moved with their owners. Relative links,
  reproduction commands and the Bot library repository URL were updated.
- All 276 relocated tracked files were accounted for. Firmware `src/` and
  `include/` contents were compared byte-for-byte with the pre-migration revision
  and are unchanged. Original vendor licenses/assets were retained.

## Verification

Commands run from repository root, unless noted:

| Check | Result |
| --- | --- |
| `sh tools/check_host.sh` | Passed all existing contracts, including source-reading font checks and native Bot/orb rendering |
| `pio run -d projects/m5stack-stopwatch/bot-ux-watch` | Success; shared libraries discovered and linked |
| `pio run -d projects/m5stack-core2/bot-ux-codex-core2` | Success; shared libraries discovered and linked |
| `pio run -d projects/m5stack-core2/pps` | Success; private PPS driver discovered and linked |
| `render.sh` from the relocated host-preview directory, without output argument | Success; output under root `tmp/botux-preview/` |
| Native catalog capture and `catalog.py` packaging | 32 entries; GIF/PNG assets and standalone HTML unchanged; Markdown reproduction paths updated |
| Local Markdown link resolution | Passed after migration and catalog regeneration |
| Python parsing, shell syntax, `git diff --check` | Passed |

Catalog packaging used Pillow in an isolated, ignored environment under
`tmp/repository-layout/venv/`. Build/check logs are disposable files in
`tmp/repository-layout/`; this record preserves the outcome without requiring them.
No device flashing or new physical acceptance was performed for the directory move.
