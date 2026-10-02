# Corallium factory derivative

- Edit overlay files or the reviewed patch; never commit generated upstream
  checkouts, managed_components, SDKs, binaries or logs. Keep scratch under tmp/.
- Preserve upstream notices; UPSTREAM_LICENSE is the factory Apache-2.0 license.
- Keep the versioned root protocols/corallium-v1 contract authoritative. RX and
  CCCD permissions require encryption; only a physical UI action opens the BLE
  window. Never log credential payloads or advertise on every startup.
- Changes to the patch must pass git apply --check against upstream.lock.json.
- Validate portable code with tools/check_host.py; attempt the firmware build.
  Neither host checks nor successful compilation establish hardware acceptance.
- Native renderer statistics measure actual frames. Never describe target tick
  intervals as achieved FPS or nominal capacity as measured remaining runtime.
