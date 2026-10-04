# M5Stack StopWatch device

- Read `specs/hardware.md` for the known display, touch, RTC, audio and power setup.
- Keep hardware facts here; companion timing, UI, settings and input semantics
  belong to `bot-ux-watch/AGENTS.md` and its project specs.
- The current app pins M5Unified/M5GFX/PM drivers and applies project-local patches.
  Preserve those pins and patches when moving files or adding shared libraries.
- Do not infer acceptance from a connected serial port or diagnostic contact
  injection. Distinguish native raster, SDK/host tests and physical interaction.
