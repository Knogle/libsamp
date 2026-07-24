# Progressive overlay: RPC178 `ScrSetActorHealth`

## Scope and identity

This is the first function-level `samp_re.asi` replacement. It is restricted
to the original SA-MP 0.3.7-R5 DLL:

```text
SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
PE timestamp=0x6372c39e
preferred image base=0x10000000
entry RVA=0x000cbc90
SizeOfImage=0x0027e000
machine=IMAGE_FILE_MACHINE_I386
section count=5
```

`samp_re` computes the actual loaded module file's SHA256 with WinCrypt before
it considers a patch. The PE fields, relocated handler prologue, and
downstream method prologue are separate runtime gates.

## Static contract

`STATIC_037`: bounded disassembly/Ghidra metadata for the DLL above identifies
RPC178 at `samp.dll+0x0001dbe0`, function extent
`[0x0001dbe0,0x0001dcb4)`. Its first two complete instructions are:

```text
6a ff                   push -1
68 <base+0x000e16bb>    push relocated SEH metadata
```

The entry patch is exactly seven bytes:

```text
original: 6a ff 68 <relocated-u32>
patch:    e9 <rel32-to-overlay> 90 90
length:   7
```

The trampoline copies those relocated seven bytes and jumps to
`samp.dll+0x0001dbe7`. Installation checks the target again after changing
page protection, flushes the instruction cache, verifies the complete patch,
restores the original protection, and only then publishes the live state.
Every failure after a write attempts to restore the saved bytes before the
hook becomes active.

`STATIC_037`: the valid handler path reads a raw little-endian `uint16`
followed by 32 float bits and then resolves:

```text
*(samp.dll+0x0026eb94) -> NetGame
NetGame+0x03de         -> Pools
Pools+0x10             -> ActorPool

ActorPool+0x0004+id*4  -> CActor pointer
ActorPool+0x0fa4+id*4  -> active state
id capacity            = 1000
```

It calls `CActor::SetHealth` at `samp.dll+0x0009c5d0`, whose verified entry is
`8b 41 48 85 c0`. The replacement passes the health word as an unmodified
32-bit stack argument under the x86 `thiscall` ABI; this avoids normalizing
NaNs or otherwise changing float payload bits. A diagnostic readback follows
the same statically verified `CActor+0x48 -> CPed`, `CPed+0x540 -> health`
path and records the stored raw bits after the downstream call.

The original handler only explicitly checks ActorPool, actor-ID bounds, active
state, and the CActor pointer after dereferencing NetGame and Pools. The
overlay additionally checks that each pointer read is mapped. If the
NetGame/Pools chain cannot be read, it invokes the original handler once
without having mutated state. This defensive fallback is `INFERRED` and
`TODO_VERIFY`; it is not expected during a valid registered RPC callback.

## Modes and fallback

- Default/bypass: no code is patched.
- Shadow: the overlay classifies/logs the input and calls the original
  trampoline exactly once.
- Replace: exactly 48 readable bits use the replacement decode/pool dispatch
  and original downstream `CActor::SetHealth`.
- A malformed, non-48-bit, or unreadable input calls the original trampoline
  exactly once. Inactive/out-of-range/null Actor slots reproduce the observed
  original no-op branches and do not call the original handler again.

Activation requires both `SAMP_RE_MODE=shadow|replace` and
`SAMP_RE_FUNCTIONS=rpc178_set_actor_health`. `SAMP_RE_DISABLE=1` and the fixed
`samp_re_disabled.flag` are kill switches. Actor-probe flags/environment
switches are treated as conflicts because that probe patches the same entry.
The worker also requires a writable `samp_re.log` before any patching.

## Restore path

The ASI pins itself and the original DLL before installing. If
`samp_re_disabled.flag` appears during a run, the worker atomically changes
the hook state to live bypass. Every subsequent callback invokes the original
trampoline exactly once; the trampoline and entry patch remain valid for
process lifetime.

The physical seven-byte restore is process teardown, or removal/redeployment
of the ASI while the lab scripts have verified that no GTA/SA-MP process is
running. The overlay intentionally does not rewrite live handler bytes during
the kill-switch transition: a seven-byte x86 write is not atomic, and an
active thread can be between entry fetch and the overlay's call counter. This
is the conservative restore path until thread-quiescence logic has direct
runtime evidence. During install rollback, original bytes are written only
while the target still equals this overlay's exact patch; an ownership
mismatch is logged and left untouched.

## Evidence and open validation

- Handler RVA, extent, relocated prologue, payload reads, pool chain, branch
  order, downstream RVA, and downstream prologue: `STATIC_037`.
- Relocated seven-byte Actor prologue behavior under Wine:
  `PROBE_TRACE` (original Actor probe run documented in
  [actor_probe_runbook_20260716.md](actor_probe_runbook_20260716.md)).
- Original, overlay-shadow, and overlay-replace behavior for valid positive
  `/rpcactors` payloads on native Windows: `OBSERVED_037 + PROBE_TRACE`;
  documented in
  [rpc178_samp_re_parity_20260723.md](../traces/rpc178_samp_re_parity_20260723.md).
- Exact raw CPed-health readback, original-handler suppression in replace
  mode, and permanent live disabled-flag bypass: `PROBE_TRACE`.
- Malformed input, missing/inactive pool slots, nonpositive health, NaN, and
  downstream helper side effects: `TODO_VERIFY`.
- A seven-byte x86 entry rewrite is not an atomic instruction update. The
  worker installs before expected RPC traffic and publishes its callable
  trampoline before writing, but a native stress run must still verify that
  the chosen launch point is quiescent. Thread suspension is intentionally not
  introduced without evidence that it is needed: `TODO_VERIFY`.
