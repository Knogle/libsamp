# SPEC-NET-004: TCP Bootstrap Manager Lifecycle

## Status

- Draft
- Spec owner: Team A
- Consumer: Team B

## Goal

Capture lifecycle behavior of the mapped network manager object family (`0x10055e60..0x10057200`) so Team B can reimplement equivalent start/stop/connect/listen orchestration.

## Mapped Cluster (Current Binary)

1. `fcn.10055e60`: constructor-style init sequence + `WSAStartup` + member defaults.
2. `fcn.10055f30`: destructor/stop sequence (socket close, worker flags, container teardown).
3. `fcn.10056fc0`: object creation/bootstrap path invoking init sequence.
4. `fcn.10056880`: listen/start path (`socket/bind/listen`) and thread/handle handoff.
5. `fcn.10055ff0`: outbound connect worker path (resolve + socket + connect).
6. `fcn.10057200`: shutdown/release wrapper around stop/destructor pieces.
7. `fcn.10056300`: owner destructor cascading through child teardown calls.

## Required Lifecycle Semantics

1. Deterministic initialization:
   - allocate internal containers,
   - set status flags to known defaults,
   - initialize socket runtime once.
2. Start/listen path:
   - reject duplicate active starts,
   - create/bind/listen socket and transition state to running,
   - launch worker execution context.
3. Outbound connect path:
   - resolve host before socket connect,
   - on failure, close temporary socket and propagate failure state.
4. Stop path:
   - clear running flags,
   - close active sockets first,
   - wait for worker stop gate,
   - release container-managed socket/client objects.
5. Destruction path:
   - idempotent if already stopped,
   - release nested allocators/containers in reverse init order.

## Reimplementation Guidance

1. Expose this as a narrow internal module API; avoid leaking object layout.
2. Keep worker and socket ownership explicit to avoid teardown races.
3. Ensure stop/destruct is safe when partially initialized.
4. Track state transitions with test-visible counters/flags.

## Reimplementation Progress

1. `samp_tcp_bootstrap_manager` scaffold is implemented in `reimpl/src/net/tcp_bootstrap_manager.c`.
2. Legacy listen/connect path remains available for deterministic host tests.
3. Optional RakNet client lifecycle is provisioned through adapter-backed APIs:
   - `samp_net_mgr_connect_raknet`
   - `samp_net_mgr_raknet_is_connected`
   - `samp_net_mgr_raknet_pump`
4. Full worker-thread parity and reconnect cadence are still pending.
5. Detailed object/worker breakdown is now tracked in `SPEC-NET-006`.

## Test Matrix (Minimum)

1. Init -> Start -> Stop -> Destroy normal sequence.
2. Init -> Destroy without Start.
3. Double-Start refusal behavior.
4. Stop called twice (idempotent).
5. Connect failure closes temporary socket resources.
6. Listener start with ephemeral port supports local accept/connect smoke test.

## References

- `analysis/notes/fcn_10055e60_auto.txt`
- `analysis/notes/fcn_10055f30_auto.txt`
- `analysis/notes/fcn_10056fc0_auto.txt`
- `analysis/notes/fcn_10056880_auto.txt`
- `analysis/notes/fcn_10057200_auto.txt`
- `analysis/notes/fcn_10056300_auto.txt`
