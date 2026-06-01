# SPEC-NET-006: Network Object Lifecycle and Connect/Listen Worker Cluster

## Status

- Draft
- Spec owner: Team A
- Consumer: Team B

## Goal

Define clean-room behavior for the mapped network object cluster around:

- `fcn.10055e60` (constructor/init),
- `fcn.10055f30` (stop/destructor core),
- `fcn.10055ff0` (connect worker),
- `fcn.10056880` (listen/start worker).

This spec is the bridge from low-level socket primitives (`SPEC-NET-003`) and RakNet integration (`SPEC-NET-005`) to a full runtime manager state machine.

## Mapped Behavior Summary

1. `fcn.10055e60`:
   - initializes multiple embedded containers/queues,
   - sets status bytes/handles to known defaults,
   - performs `WSAStartup(2.2)`.
2. `fcn.10055f30`:
   - idempotent stop path guarded by an active flag,
   - closes primary socket if valid,
   - waits for worker-active flag to clear,
   - closes sockets in a dynamic container,
   - destroys/clears all embedded containers in deterministic order.
3. `fcn.10055ff0`:
   - resolves host with `gethostbyname`,
   - opens `socket(AF_INET, SOCK_STREAM, 0)`,
   - attempts `connect`,
   - on success enqueues `{socket, ip, port}` result object into internal queue,
   - uses a flag-wait gate before final handoff.
4. `fcn.10056880`:
   - start/listen function guarded against double-start,
   - creates stream socket, binds local endpoint, performs `listen(backlog)`,
   - spawns a worker thread with object context,
   - stores thread handle then closes transient creator handle.

## Clean-Room Runtime Contract

1. Manager init path must be idempotent and fully zero/default all fields before first use.
2. Manager stop path must be idempotent and safe to call during partial startup.
3. Start/listen must fail fast when already running.
4. Connect worker must communicate completion through owned queue/state, not direct shared mutable socket fields.
5. Teardown order must guarantee:
   - worker stop request,
   - worker join/wait,
   - socket close,
   - queue/container cleanup.
6. All socket handles stored in manager-owned containers must be invalidated after close.

## Data Model (Reimplementation Target)

Minimum fields to converge toward mapped semantics:

1. lifecycle flags:
   - `initialized`,
   - `running`,
   - `worker_connect_active`,
   - `worker_listen_active`,
   - `stop_requested`.
2. handles:
   - primary listen socket,
   - worker thread handles/ids.
3. connection queue:
   - entries with `{socket, ipv4_be, port}`.
4. stats/counters:
   - init/start/stop counts,
   - connect attempts,
   - accepted connections.

## Verification Matrix

1. Init/stop idempotency:
   - repeated init returns success and does not leak resources,
   - repeated stop returns success.
2. Double-start guard:
   - second listen/start call returns failure.
3. Connect worker handoff:
   - successful connect appears in manager queue/state exactly once.
4. Stop during in-flight worker:
   - worker exits,
   - sockets are closed,
   - manager returns to neutral state.
5. Teardown order safety:
   - no use-after-close on sockets during shutdown path.

## References

- `analysis/notes/fcn_10055e60_auto.txt`
- `analysis/notes/fcn_10055f30_auto.txt`
- `analysis/notes/fcn_10055ff0_connect_worker.txt`
- `analysis/notes/fcn_10056880_auto.txt`
- `specs/SPEC-NET-003_LEGACY_IPV4_SOCKET_PRIMITIVES.md`
- `specs/SPEC-NET-005_KNOGLE_RAKNET_INTEGRATION.md`
