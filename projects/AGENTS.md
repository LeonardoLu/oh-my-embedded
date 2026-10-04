# Project organization

- Device directories use lowercase `brand-model`; project names describe the
  firmware or experiment. Each level has README and AGENTS.
- Keep hardware/assembly contracts in device `specs/`, app contracts and acceptance
  in project `specs/`, and reusable component contracts beside their shared library.
- Keep shared integration and verification decisions in `shared-libs/specs/`;
  project indexes link to their owners. Specs state current behavior and reasons,
  not a sequence of tasks, deployments or superseded requirements.
- Current M5Stack apps use PlatformIO/Arduino and `espressif32@6.13.0`. Respect the
  pinned dependencies in each app. New devices may use a different SDK/toolchain.
- Apps at `<device>/<project>` discover shared libraries through
  `lib_extra_dirs = ../../shared-libs`. Keep vendor-private code local when useful.
- Run root `tools/check_host.sh` for shared code/path changes and build affected
  firmware. Host evidence does not replace physical hardware acceptance.
- Read device/project AGENTS before changing code; never apply one app's input,
  power, BLE or LED policy to all projects on its host device.
