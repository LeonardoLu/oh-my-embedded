# oh-my-embedded — Repository Guide

Personal embedded-device projects across manufacturers. The repository was
previously named oh-my-m5stack; M5Stack is one device family, not a repository-wide
hardware or SDK requirement.

## Layout and ownership

- `DEVICES.md`: known devices, accessories, configurations and details to confirm.
- `projects/<brand-model>/`: device README, AGENTS and hardware `specs/`.
- `projects/<brand-model>/<project>/`: independently buildable firmware, project
  README, AGENTS, functional `specs/`, tests and validation evidence.
- `projects/shared-libs/<library>/`: reusable libraries and their own specs.
- `projects/shared-libs/specs/`: preserved cross-device companion iteration and
  acceptance records; these are historical context, not new-device defaults.
- `tools/`: repository-wide host checks and diagnostics.
- `wiki/bot-ux/`: bilingual illustrated catalog with native animation captures.
- `tmp/`: ignored scratch, research, build logs and generated previews.

Read the applicable device/project/library AGENTS before editing. Device rules
apply to all projects on that device; app behavior belongs in project AGENTS.
Use lowercase brand-model device names, e.g. `m5stack-core2`. Add README and AGENTS
at each new device and project root. Do not invent hardware ownership, quantities,
revisions or acceptance; leave unconfirmed inventory fields explicit.

## Working rules

- Match existing code style; keep comments purposeful. Do not over-engineer.
- The user's current goal and newer applicable specs supersede historical notes.
  Keep durable implementation and validation notes in the owning `specs/`;
  `tmp/` must never be the sole source of an implementation contract.
- Preserve vendor licenses and project-private dependencies. Shared components
  belong in `projects/shared-libs`; keep their actual platform limits explicit.
- Update build, test, tool, documentation and asset references when moving files.
  Commands in the root/project READMEs run from the repository root unless stated.
- Commit meaningful units on `main` or a topic branch; never track `tmp/` or
  PlatformIO caches.
- Prefer builds, automated checks and static review over manual-only acceptance.
  Connected serial ports alone do not establish hardware acceptance.
- BotUx semantics changes must keep `wiki/bot-ux/intro.md` and `intro.html` current;
  regenerate animation examples with the native host renderer, not CSS motion.
