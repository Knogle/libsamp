# Original 0.3.7-R5 Actor RPC Probe

## Scope

This runbook captures one deterministic `/rpcactors` cycle against the local
original SA-MP 0.3.7-R5 client. It is intentionally limited to the Actor RPC
handlers and must not be combined with the stale generic SA-MP network hooks.

Original client identity:

```text
samp.dll SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
```

## Static evidence

`STATIC_037`: local disassembly of the exact DLL above shows these active
`RegisterAsRemoteProcedureCall` mappings:

| RPC | Handler RVA | Meaning | Safe whole-instruction prologue |
| --- | --- | --- | --- |
| 171 | `samp.dll+0x0000eab0` | ShowActor | `6a ff 68 eb 05 0e 10` |
| 172 | `samp.dll+0x00011e00` | HideActor | `6a ff 68 cb 0a 0e 10` |
| 173 | `samp.dll+0x0001d750` | ApplyActorAnimation | `55 8b ec 83 e4 f8` |
| 174 | `samp.dll+0x0001d930` | ClearActorAnimations | `6a ff 68 5b 16 0e 10` |
| 175 | `samp.dll+0x0001d9f0` | SetActorFacingAngle | `6a ff 68 7b 16 0e 10` |
| 176 | `samp.dll+0x0001dad0` | SetActorPos | `6a ff 68 9b 16 0e 10` |
| 178 | `samp.dll+0x0001dbe0` | SetActorHealth | `6a ff 68 bb 16 0e 10` |

The six `push <SEH metadata>` immediates in the table are preferred-base bytes.
`PROBE_TRACE` from the first guarded run showed Wine loading `samp.dll` away
from `0x10000000` and applying the expected PE `HIGHLOW` relocations. The
preflight therefore validates the first three opcodes plus the invariant
referenced RVA and dynamically derives the runtime absolute value. RPC 173's
six-byte whole-instruction prologue contains no relocated immediate and remains
an exact byte comparison.

The handler ABI is `void __cdecl Handler(RPCParameters *)`. The probe reads
only the observed prefix `input` at `+0` and `numberOfBitsOfData` at `+4`,
bounds the payload to 8192 bits, logs a maximum 256-byte hex preview, and then
calls the original handler trampoline.

## Probe setup

Build through the `devbuild` toolbox:

```bash
toolbox run -c devbuild bash -lc \
  'cd /home/chairman/Projects/sa-mp.dll-rebuild && tools/asi_probe/build_win32.sh'
```

Deploy `build-asi-probe/samp_probe.asi` to the original GTA root:

```text
/home/chairman/Games/san-andreas-multiplayer-legacy-legacy/drive_c/Program Files (x86)/Rockstar Games/GTA San Andreas
```

Enable only:

```text
samp_probe_actor_hooks.flag
```

Disable unrelated deep flags, especially:

```text
samp_probe_font5_hooks.flag
samp_probe_samp_code_hooks.flag
samp_probe_gta_asset_hooks.flag
samp_probe_textdraw_render.flag
```

## Scenario

1. Start the original/reference prefix and connect to the local bare server on
   `127.0.0.1:7778`.
2. Spawn normally and execute `/rpcactors`.
3. Observe at least one complete eight-phase cycle (about 32 seconds): create,
   animation, clear/move/health, facing, stream-out, stream-in, damage window,
   destroy/recreate.
4. During the announced damage window, shoot one visible actor once.
5. Execute `/rpcactorsoff` and exit cleanly.

## Acceptance checks

- Startup contains `actor_code_hook: set_preflight_ok hooks=7` and
  `actor_code_hook: summary installed=7 requested=7`.
- Every observed handler call has an `actor_rpc: phase=begin` and matching
  `actor_rpc: phase=end` line.
- ShowActor reports 216 bits / 27 bytes and the four expected actor IDs/skins.
- Hide, animation, clear, angle, position, and health events follow the bare
  server phase messages.
- There is no probe `exception:` line and the client exits cleanly.

The first successful runtime capture upgrades the handler-entry observations
from `STATIC_037`/`TODO_VERIFY` to `PROBE_TRACE`. Visual behavior remains
`OBSERVED_037` evidence and should be compared separately with the replacement
client after its Actor pool exists.

## Heavy ActorPool / CActor / GTA trace

Use this mode when handler payloads alone are insufficient and the exact
original-client mutation path is required. Enable this flag instead of the
normal flag (heavy mode implies the seven RPC hooks):

```text
samp_probe_actor_heavy.flag
```

Keep the unrelated deep flags listed above disabled. Heavy mode remains tied
to the exact original R5 DLL hash and first preflights this complete typed
SA-MP method set:

| RVA | Static meaning | ABI | Whole-instruction prologue |
| --- | --- | --- | --- |
| `samp.dll+0x1900` | `ActorPool::New` | `int __thiscall(void*, PackedActorInfo*)` | `64 a1 00 00 00 00` |
| `samp.dll+0x16f0` | `ActorPool::Delete` | `int __thiscall(void*, uint16)` | `66 8b 44 24 04` |
| `samp.dll+0x9c460` | `CActor::ApplyAnimation` | `void __thiscall`, eight 32-bit args | `55 8b e9 8b 45 48` |
| `samp.dll+0x9c550` | `CActor::ClearAnimations` | `void __thiscall()` | `8b 49 48 85 c9` |
| `samp.dll+0x9c570` | `CActor::SetFacingAngle` | `void __thiscall(float)` | `56 8b f1 8b 46 48` |
| `samp.dll+0x9c5d0` | `CActor::SetHealth` | `void __thiscall(float)` | `8b 41 48 85 c0` |
| `samp.dll+0x9c700` | `CActor::SetInvulnerable` | `void __thiscall(bool)` | `8b 41 48 85 c0` |
| `samp.dll+0x9f040` | `CActor::SetPosition` | `void __thiscall(float,float,float)` | `55 8b ec 51 8b 41 40` |

`STATIC_037` establishes this pool chain and layout:

```text
*(samp.dll+0x26eb94) -> NetGame
NetGame+0x3de        -> Pools
Pools+0x10           -> ActorPool (0x4e24 bytes, 1000 slots)

ActorPool+0x0000             high-water actor id
ActorPool+0x0004 + id*4      CActor pointer
ActorPool+0x0fa4 + id*4      active state
ActorPool+0x1f44 + id*4      CPed pointer
ActorPool+0x2ee4 + id*4      per-slot flag A
ActorPool+0x3e84 + id*4      per-slot flag B
```

The original `CActor` allocation is exactly `0x56` bytes. Relevant statically
observed members are entity `+0x40`, GTA actor handle `+0x44`, CPed `+0x48`,
and invulnerability byte `+0x55`. The probe dumps this bounded object plus
three bounded CPed windows and the transformation matrix before and after each
stage. In particular, facing and positive health do not call a GTA function:
the R5 code writes `CPed+0x55c` and `CPed+0x540` directly.

The local GTA executable identity used for downstream validation is:

```text
gta_sa.exe SHA256=a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26
```

| GTA address | Meaning | Whole-instruction prologue |
| --- | --- | --- |
| `0x5e4110` | `CPed::Teleport(x,y,z,reset)` | `56 8b f1 8b 86 98 05 00 00` |
| `0x601640` | `CPedIntelligence::FlushImmediately(bool)` | `6a ff 68 26 e8 83 00` |
| `0x469eb0` | `CRunningScript::ProcessOneCommand()` | `66 ff 05 f4 47 a4 00` |

The script dispatcher is extremely hot process-wide. The hook therefore emits
`actor_heavy_scm` only while a typed Actor hook owns the TLS scope and only for
caller `samp.dll+0xb22ee`, the statically proven return site of the SA-MP SCM
bridge. Expected actor opcodes include `009a` create, `0173` heading, `0446`
critical hits, `060b` decision maker, `0812` animation, `0321` explode head,
`02ab` proofs, and train-only `07c7` positioning.

Heavy acceptance markers:

- `actor_code_hook: set_preflight_ok hooks=7`;
- `actor_heavy_hook: set_preflight_ok samp_hooks=8`;
- `actor_heavy_gta_hook: set_preflight_ok hooks=3`;
- matching `rpc.begin/rpc.end`, `method.begin/method.end`, and GTA/SCM
  begin/end records for the exercised phases;
- no `incomplete_*`, `exception:`, or missing clean process detach.

The Heavy hooks are `STATIC_037`/`GTA_REVERSED_REF` until the first clean
original run. Only that run may promote the actual call and field transitions
to `PROBE_TRACE`.
