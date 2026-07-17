# Actor Runtime And Combat Compatibility (2026-07-17)

## Scope and binary identity

This note records the diagnosis of the replacement `/rpcactors` run with
build ID `Jul 17 2026 12:19:36`, the corresponding Actor mutation fix, and the
original-client evidence used for BulletSync and ActorDamage compatibility.
All original-client claims below are tied to these exact binaries:

```text
samp.dll   0.3.7-R5 SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
gta_sa.exe USA 1.0 SHA256=a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26
```

The diagnosed replacement run is identified by its logged build ID, not by a
preserved DLL hash. The post-fix candidate built and installed in the libsamp
test prefix is
`SHA256=ee87297698a805140d499290de9ebf8c4d094953e9b9418e9587a1fc0d55e864`.
Tie that hash to the next runtime trace before promoting any new behavior from
`TODO_VERIFY`.

## Diagnosed replacement run

`PROBE_TRACE`: the relevant `process_attach` block is
`samp_runtime.log:398444-399278`; it ends in `process_detach: done` and has no
current `exception_filter` marker. The matching Actor traffic is in the same
run's `samp_net_trace.log` block.

- Phase 2 received all four RPC 174/176/178 sequences in the observed order
  ClearAnimations -> SetPosition -> SetHealth. The decoded positions and
  health values `100/85/70/55` are correct.
- Phase 3 received the four intended headings `45/315/225/135` degrees.
- Phase 4 received four HideActor RPCs. The runtime logged four deletes and
  later four creates on stream-in.
- The server recorded 43 replacement `OnPlayerWeaponShot` callbacks at
  `omp-server-bare/log.txt:28013-28090`, all with weapon 31, hit type `NONE`,
  ID `0xffff`, and callback coordinates `(0,0,0)`. The client put a synthetic
  camera ray in BulletSync `hit_position`, but left the packet `offset` at
  zero; open.mp exposes that offset as the callback coordinates for a `NONE`
  hit.
- No ActorDamage RPC 177 was emitted in that replacement run.

The Actor RPC decoder is therefore not the phase 2-4 failure. The replacement
accepted attempted GTA mutations as complete without reading the resulting
physical state back.

## Actor mutation diagnosis and correction

The compiled replacement implementation of
`actor_compat_apply_position_physical` used ESP-relative memory operands while
constructing the `CPed::Teleport` call with successive pushes. Each push moved
ESP before the next operand was read. The generated sequence therefore passed
unrelated stack values instead of the requested x/y/z coordinates, returned
success, and consumed the position revision. This diagnosis is a direct
inspection of the replacement object code; its visual correlation remains
`TODO_VERIFY` until the post-fix run.

`STATIC_037 + PROBE_TRACE + OBSERVED_037`: original
`CActor::SetPosition` at `samp.dll+0x0009f040` calls the CPed virtual method at
vtable `+0x38`; for the pinned GTA build this resolves to
`gta_sa.exe+0x001e4110` (`CPed::Teleport`). Original facing and positive-health
changes write `CPed+0x55c` and `CPed+0x540` directly. ClearAnimations obtains
the intelligence pointer from `CPed+0x47c` and calls
`gta_sa.exe+0x00201640` (`CPedIntelligence::FlushImmediately(true)`).

The replacement now uses an explicit typed `__thiscall` for Teleport and only
consumes pending revisions after physical confirmation:

| Mutation | Confirmation before revision advance | Evidence/status |
| --- | --- | --- |
| Position | Matrix x/y/z within `0.05` of the target | `STATIC_037 + PROBE_TRACE`; post-fix `TODO_VERIFY` |
| Facing | `CPed+0x55c`, wrapped angular delta at most `0.001` radians | `STATIC_037 + PROBE_TRACE`; post-fix `TODO_VERIFY` |
| Health | `CPed+0x540`, absolute delta at most `0.001` | `STATIC_037 + PROBE_TRACE`; post-fix `TODO_VERIFY` |
| ClearAnimations | expected task-manager root pattern after `FlushImmediately(true)` | `STATIC_037 + GTA_REVERSED_REF`; semantic readback `TODO_VERIFY` |
| Delete | opcode 009B followed by GTA ped-pool handle absence | `INFERRED + TODO_VERIFY` against original `ActorPool::Delete` |

Snapshot application attempts the observed phase-2 Clear -> Position -> Health
order, but completes the individual RPC revisions independently. A
non-abortable GTA task can therefore leave only ClearAnimations pending without
starving Position, Facing, or Health. A failed readback leaves its revision
pending and emits a bounded retry marker. Initial Actor creation also confirms
position, facing, and health before marking those individual revisions
applied. The exact original delete path uses the SA-MP `CActor` destructor and
ActorPool slots; opcode 009B plus pool readback is a conservative replacement
and is not claimed as parity.

## Original BulletSync and ActorDamage behavior

`STATIC_037`: `samp.dll+0x000a5410` builds BulletSync from the actual GTA
line-of-sight result. Its origin is the line start and its hit position is
`CColPoint::point`. Remote players, vehicles, and objects map to hit types
1/2/3 with an entity-local offset. An Actor is deliberately not one of those
mapped entities: its packet remains type `NONE`, ID `0xffff`, while both
`hit_position` and `offset` contain the world collision point. If GTA reports
no victim, the collision point is uninitialised and must not be read; using the
line end for the replacement miss position remains `TODO_VERIFY`.

`OBSERVED_037 + PROBE_TRACE`: the comparable original run at
`omp-server-bare/log.txt:27602-27621` shows Actor hits as
`OnPlayerWeaponShot(..., NONE, 0xffff, real-world-impact)`. Each actual Actor
hit is accompanied by ActorDamage RPC 177 with Actor ID, AK-47 damage `30.0`,
weapon 31, and the GTA body part.

`STATIC_037 + OPENMP_REF`: `samp.dll+0x000a3bb0` serializes RPC 177 (write
site `samp.dll+0x000a3cda`) as:

```text
bool taking=false (one bit)
uint16 actor_id
float damage
uint32 weapon
uint32 bodypart
```

The original send at `samp.dll+0x00006b76` uses `HIGH_PRIORITY`,
`RELIABLE_ORDERED`, channel 0. open.mp independently reads the same layout in
[`Shared/NetCode/actor.hpp`](https://github.com/openmultiplayer/open.mp/blob/master/Shared/NetCode/actor.hpp)
and dispatches Actor damage from the Actor component.

## Replacement GTA entry hooks

Both hooks are limited to the pinned GTA USA 1.0 build and require exact entry
bytes before patching. Their trampolines copy the complete displaced
instructions and jump to the stated resume RVA. The entries and ABIs are
`STATIC_037 + GTA_REVERSED_REF`; replacement installation and behavior remain
`TODO_VERIFY` until the post-fix runtime trace.

| Hook | Module + RVA | Original bytes | Patch bytes | Length | Resume RVA |
| --- | --- | --- | --- | --- | --- |
| Bullet impact | `gta_sa.exe+0x0033b550` | `6a ff 68 50 8e 84 00` | `e9 <rel32> 90 90` | 7 | `gta_sa.exe+0x0033b557` |
| Damage response | `gta_sa.exe+0x000b5ac0` | `64 a1 00 00 00 00` | `e9 <rel32> 90` | 6 | `gta_sa.exe+0x000b5ac6` |

### Bullet-impact ABI and behavior

The first hook is `CWeapon::DoBulletImpact`, a `__thiscall` with `ECX` as the
`CWeapon *` and these stack arguments: fired-by entity at `+0x04`, victim at
`+0x08`, line start at `+0x0c`, line end at `+0x10`, `CColPoint *` at
`+0x14`, and incremental-hit flag at `+0x18`; the original returns with
`ret 0x18`. The pinned `CColPoint` is `0x2c` bytes, with world point at
`+0x00` and victim piece/body-part byte at `+0x24`.

The stub preserves registers, reports local-player shots from the actual GTA
collision result, then always continues through the trampoline. It emits Actor
hits as `NONE/0xffff` with the world point in both hit and offset; mapped
players, vehicles, and objects retain types 1/2/3 and entity-local offsets.
When this hook is installed the older 150 ms ammo-poll BulletSync path is
disabled so automatic fire is neither dropped nor duplicated. Exact miss and
non-Actor mapping parity are `TODO_VERIFY`.

### Damage-response ABI and behavior

The second hook is
`CPedDamageResponseCalculator::ComputeDamageResponse`, a `__thiscall` with
`ECX` as the `0x14`-byte calculator and stack arguments victim at `+0x04`,
response at `+0x08`, and speak flag at `+0x0c`; the original returns with
`ret 0x0c`. The calculator fields used are damager `+0x00`, damage float
`+0x04`, body part `+0x08`, weapon `+0x0c`, and speak byte `+0x10`.

`STATIC_037 + PROBE_TRACE`: when the damager is the local ped and the victim
is a streamed Actor, the callback sends RPC 177 and returns directly with
`ret 0x0c`, matching the original client's suppression of local Actor health
damage. Non-Actor calls continue through the trampoline. Recognised Actor
damage remains locally suppressed even if the network send fails, preventing
client-side Actor death from diverging from the server state. Post-fix runtime
confirmation remains `TODO_VERIFY`.

### Restore path

Before either entry jump is published, the replacement pins `samp.dll` with
`GetModuleHandleEx(FROM_ADDRESS | PIN)`. Failure to pin disables both hook
installs. The hooks and their generated code are therefore process-lifetime:
on an explicit guarded restore, each hook reconstructs its own `E9`/NOP patch
and restores the saved original bytes only if the live entry still equals that
patch, but deliberately retains its stub and trampoline. A CPU may already
have followed the old entry jump when the entry bytes are restored, so freeing
that code would be unsafe. If another component changed the entry, restoration
is skipped and the allocations likewise remain valid so an unknown hook chain
is not invalidated.

Both callbacks check a shared shutdown gate before accessing runtime/network
state. During process termination, `DLL_PROCESS_DETACH` only closes that gate;
it does not log through CRT stdio, wait, restore entry bytes, free executable
code, or destroy RakNet while holding the loader lock. Windows reclaims the
process address space. A true hot-unload path is not supported while either GTA
entry hook is installed. Install and guarded restore outcomes are logged with
module RVA, original bytes, patch form, length, and reason.

## Post-fix acceptance (`TODO_VERIFY`)

- Startup logs both `bullet_impact_hook: installed` and
  `actor_damage_hook: installed`; no version/byte preflight skip occurs.
- Phase 2 logs confirmed clear, position, and health readbacks for all four
  Actors, with no persistent retry marker; visual positions move outward.
- Phase 3 headings survive phase 4 stream-out and are visible after phase 5
  stream-in. Phase 4 pool readback confirms all four peds absent.
- Every automatic shot produces one BulletSync packet. Actor impacts remain
  type `NONE`, ID `0xffff`, with the real, nonzero world point in both packet
  hit/offset and the server callback.
- Each Actor impact also produces exactly one RPC 177 and one server Actor
  damage callback with matching Actor ID, damage, weapon, and body part.
- The run has no `exception_filter` and no freeze last-marker. A normal process
  exit intentionally emits no `process_detach` stdio marker and does not
  restore or free the process-lifetime hooks; use the terminal game/network
  markers to distinguish a clean exit from a freeze.
