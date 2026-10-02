# Corallium

- Keep a single SwiftUI application target for macOS and iOS. Preserve the owner's
  bundle identifier and signing team; local unsigned builds may override signing.
- macOS defaults to Automatic / Apple Development signing so code identity survives
  rebuilds. Ad-hoc signing must be explicit and use a separate build directory;
  never silently downgrade the app used for Bluetooth permission validation.
- Protocol definitions live in `../../protocols`; capabilities come from the
  connected firmware, never from a model-name assumption.
- BLE mutations are explicit user actions. One request in flight, bounded framing,
  response timeout, no automatic mutation retries, no credentials in logs.
- Daily JSONL logs belong under Application Support/<bundle identifier>/logs.
  The macOS sandbox may place Application Support inside the app container.
- Demo mode must remain visually distinct and must never send BLE traffic.
- AppIcon.icon is a native Icon Composer document; preserve its layers/resources.
- Prefer native SwiftUI controls and accessibility labels. Record upstream device
  image source URLs and rights without inventing a license.
