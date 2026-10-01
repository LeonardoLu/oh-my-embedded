# Actual M5Unified touch transition probe

This host probe compiles the installed `Touch_Class.cpp` and includes its original
`Touch_Class.hpp`. The tiny `include/M5GFX.h` stub supplies only coordinates and
unused display methods. It contains no touch state machine. The test directly
exposes the SDK's protected `update_detail` method, so it tests SDK contact
transitions, not the application's diagnostic pointer injection.

Run from the repository root after PlatformIO has installed Watch dependencies:

```sh
SDK_TOUCH=projects/m5stack-stopwatch/bot-ux-watch/.pio/libdeps/m5stack-stopwatch/M5Unified/src/utility
mkdir -p tmp/sdk-touch-probe
c++ -std=c++11 -Wall -Wextra -Werror \
  -Iprojects/shared-libs/ux-components/test/sdk_touch/include -I"$SDK_TOUCH" \
  projects/shared-libs/ux-components/test/sdk_touch/probe.cpp "$SDK_TOUCH/Touch_Class.cpp" \
  -o tmp/sdk-touch-probe/probe
tmp/sdk-touch-probe/probe --expect-legacy-bug
```

Set `SDK_TOUCH` to another installed SDK utility directory to compare versions.
Without `--expect-legacy-bug`, the probe reports whether recontact is lost or
recovered. This is historical evidence, not a requirement that future SDKs retain
the defect, and it is not an application production dependency. A fixed SDK should
change the report; remove the legacy expectation when validating that upgrade.

Observed on Watch's pinned M5Unified revision
`8530f5377d782e4a25a6c482de2e71c3f75ca8eb`:

- Flick release state 10 followed immediately by a new contact becomes state 8.
- Drag release state 14 followed immediately by a new contact becomes state 12.
- Both states report no press throughout the new contact. An intervening
  no-contact update returns the state to `none`, allowing the next press.
- Release retains its stored coordinates. Movement smaller than the default
  8 px SDK flick threshold leaves those coordinates at the initial press.

The SDK clears the change bit but retains the moving bit on recontact, skips
its touch-begin branch, and does not restore the touch bit. The Watch's raw
contact sampler avoids this SDK gesture state; its application pointer tests
validate the replacement independently.

Pinned source SHA-256:

```text
47fea9d45d0bd4835a266879a13265ea2e6060e31b46f364dedeeb3304423081 Touch_Class.cpp
ce85e3fed920cae1c2aaa832bd6f06a98cf12cccb70ef5b22741d89a5539f694 Touch_Class.hpp
```

This probe does not emulate CST820 I2C sampling or prove hardware edge timing.
