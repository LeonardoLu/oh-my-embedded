# UX components

- Keep text, shapes, input, pointer, keyboard and sound selectively usable.
  Preserve the fixed-buffer sound sender and caller-owned canvas/frame loop.
- Rendering uses native RGB565 coverage. Do not replace it with CSS or rescaled
  bitmap fonts in evidence. Keep generated font sources, corpus and OFL notice.
- Public headers and `specs/ux-components-library.md` define the component
  contract; device applications own settings, persistence and power policy.
- Source-reading tests run from repository root. Update their paths when moving
  consuming apps; run `tools/check_host.sh` from root and build both consumers.
- Keep detailed sound lifetime/queue rules in `SOUND.md`; the SDK touch probe is
  historical regression evidence, not the application's contact implementation.
