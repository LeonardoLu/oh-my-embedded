# App/device contracts

- Versioned wire contracts live here; firmware and app behavior stays with its owner.
- A transport implementation must match the UUIDs, byte limits, framing, units,
  capability names and examples together. Update all peers when these change.
- Never put real network credentials, device identifiers or captured private traffic
  in fixtures. Unknown readings are null, never fabricated zeroes.
- Compatibility is established by schema/codec checks and actual transport tests;
  a simulated peripheral is not hardware acceptance.
