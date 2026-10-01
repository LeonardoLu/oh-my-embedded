# Shared libraries

- Keep libraries independent and selectively linkable. Hosts own displays, input,
  persistence, device power and frame loops. Do not move device policy into a library.
- Library API/header and library specs define component behavior. Shared `specs/`
  define integration ownership and verification limits. Update facts and reasons
  in place; do not accumulate implementation journals or deployment histories.
- Both existing companion apps consume these libraries through
  `../../shared-libs`; keep headers, library metadata and tool paths coherent.
- BotUx is still M5Canvas-based; portability must be implemented and verified
  before claiming support for a new manufacturer or SDK.
- For shared changes run root `tools/check_host.sh` and build both companion apps.
  Native previews and catalog generation must write scratch artifacts under root
  `tmp/`, and durable catalog assets under root `wiki/bot-ux/`.
