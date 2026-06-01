# SPEC-NET-005: Knogle RakNet Variant Integration

## Status

- Draft
- Spec owner: Team A + Team B

## Goal

Integrate the modified RakNet 2.52 variant from `https://github.com/Knogle/RakNet` into the clean-room rebuild as the primary compatibility networking backend candidate.

## Scope

1. Vendor RakNet source into repository under `third_party/raknet-knogle`.
2. Build static `raknet` target as part of `reimpl` build graph.
3. Keep integration optional/toggleable with CMake option.
4. Provide a stable C bridge layer for Team-B runtime code to consume without exposing RakNet internals to all modules.

## Implementation Requirements

1. `reimpl` CMake must support:
   - `SAMPDLL_ENABLE_RAKNET_KNOGLE=ON|OFF`.
2. RakNet build must not hard-require open.mp server internals (`OMP-SDK` / query component paths) for client-rebuild usage.
3. Bridge module must at least validate:
   - factory allocation/destruction for `RakPeerInterface` and `RakClientInterface`.
4. Host tests include a RakNet bridge smoke test.

## Current Integration State

1. Vendored source path exists:
   - `third_party/raknet-knogle/`
2. CMake integration implemented:
   - `reimpl/CMakeLists.txt` adds subdirectory and bridge target when enabled.
3. Bridge API implemented:
   - `reimpl/include/sampdll/net/raknet_bridge.h`
   - `reimpl/src/net/raknet_bridge.cpp`
4. Smoke test implemented:
   - `reimpl/tests/test_raknet_bridge.c`

## Next Steps

1. Replace scaffolded `tcp_bootstrap_manager` internals with RakNet-backed `Initialize/Connect/Disconnect/Receive` flow.
2. Map and implement `CNetGame`-adjacent state transitions around RakNet events.
3. Add behavior-diff tests between reference binary and rebuild for:
   - connect attempt cadence,
   - connection accepted/rejected paths,
   - reconnect flow.

## References

- `third_party/raknet-knogle/README.md`
- `reimpl/include/sampdll/net/raknet_bridge.h`
- `reimpl/src/net/raknet_bridge.cpp`
- `reimpl/tests/test_raknet_bridge.c`
