# LiquidDuck source and license

Author: nongxl. Source: https://github.com/nongxl/LiquidDuck_ESP32
Branch: `StickS3`; revision: `2922bf5d964886fae4e69dad659bee05d8b61bfd`.

`flip.cpp` and `flip.h` are unchanged copies of `src/flip.cpp` and `src/flip.h`.
The palettes and density mappings in `../liquid_engine.c` derive from
`src/main.cpp`. The upstream README declares **PolyForm Noncommercial License
1.0.0**, for noncommercial use only. That revision contains no separate LICENSE
file. This notice grants no additional rights and does not change those terms.
The factory's Apache-2.0 notices remain with their respective source files.

The native Works integration uses the factory GSP Canvas and BMI270 provider.
It adapts the tank to Mosaico's square screen, bounds elapsed time and acceleration,
initializes water at the bottom, and draws liquid with bilinear density sampling.
M5/Arduino and MicroPixel APIs, characters, audio, vibration and power controls
are not imported.
