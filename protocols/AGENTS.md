# App/device contracts

- Versioned wire contracts live here; firmware and app behavior stays with its owner.
- A transport implementation must match the UUIDs, byte limits, framing, units,
  capability names and examples together. Update all peers when these change.
- BLE admission is device-specific: StopWatch uses its timed local window;
  Mosaico defaults off and persists a manual switch with no expiry. Keep these
  profiles distinct. StopWatch retains its encrypted BLE profile; Mosaico uses
  ordinary unpaired, unencrypted GATT, including Wi-Fi credential transport.
  Mosaico must not initiate or accept pairing. Neither device may log credentials.
- Never put real network credentials, device identifiers or captured private traffic
  in fixtures. Unknown readings are null, never fabricated zeroes.
- Compatibility is established by schema/codec checks and actual transport tests;
  a simulated peripheral is not hardware acceptance.
