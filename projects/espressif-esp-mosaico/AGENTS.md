# ESP-Mosaico device rules

- ESP-Mosaico is Espressif ESP32-S31, not ESP32-S3. Use the pinned ESP-IDF
  toolchain and factory-derived source described in each project's README.
- Use the identified unit and evidence limits in `specs/hardware.md`. Never infer
  revision from a USB port or treat an eFuse/BSP version as a silkscreen inspection.
  Preserve the factory revision selection and its official pin mappings.
- GPIO60 controls display power as well as expansion rails; do not disable it
  as a blanket accessory power optimization. GPIO57 is whole-device shutdown.
- BQ27220 readings are cell telemetry. Neither USB input power nor guaranteed
  battery runtime can be inferred from the nominal 65 mAh specification.
- There is no confirmed independently backed external RTC. RTC-domain time
  survives supported resets/sleep, not removal of SoC power.
