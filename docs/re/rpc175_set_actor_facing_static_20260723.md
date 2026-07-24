# RPC 175 SetActorFacingAngle: focused R5 static inspection

## Scope and identity

This note is a bounded static inspection of the original SA-MP 0.3.7-R5
RPC 175 handler, its immediate `CActor::SetFacingAngle` target, and the angle
conversion helper used before the GTA ped field write. It contains no
decompiler pseudocode.

```text
samp.dll SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
samp.dll MD5=5ba5f0be7af99dfd03fb39e88a970a2b
format=PE32 i386
preferred image base=0x10000000
Ghidra language=x86:LE:32:default
Ghidra compiler spec=windows
```

Evidence in this note is `STATIC_037` unless a statement is explicitly marked
otherwise. Runtime behavior and final field bits still require a comparable
original-client probe.

## Reproducible artifacts

The focused script is
[`tools/ghidra/InspectActorSetFacingAngle.java`](../../tools/ghidra/InspectActorSetFacingAngle.java).
It emits identity, function, instruction, call, branch, data-reference,
scalar, incoming-reference, and bounded raw-memory TSV files under:

```text
analysis/generated/ghidra_rpc175_20260723/
```

Reproduce the final run after creating the temporary parent directory:

```bash
tools/ghidra/analyze-headless.sh /tmp/ghidra-rpc175 rpc175-r5 \
  -import '/home/chairman/Games/san-andreas-multiplayer-legacy-legacy/drive_c/Program Files (x86)/Rockstar Games/GTA San Andreas/samp.dll' \
  -scriptPath '/home/chairman/Projects/sa-mp.dll-rebuild/tools/ghidra' \
  -postScript InspectActorSetFacingAngle.java \
    '/home/chairman/Projects/sa-mp.dll-rebuild/analysis/generated/ghidra_rpc175_20260723' \
  -deleteProject
```

The verification run completed import, analysis, the focused post-script, and
post-analysis successfully with Ghidra 12.1.2. Ghidra emitted its unrelated
optional `WindowsResourceReference.java` analyzer warning; the warning did not
abort analysis or the post-script. `-deleteProject` removed the temporary
project after the export.

## RPC registration, ABI, and payload

The registration sequence at `samp.dll+0x1e70d` pushes the handler address
`samp.dll+0x1d9f0` and the address of the RPC identifier at
`samp.dll+0xe6260`, then invokes the RakNet registration method through
vtable slot `+0x58`. The identifier bytes are:

```text
samp.dll+0xe6260: af 00 00 00
```

This directly establishes the mapping to RPC 175 (`0xaf`).

The handler occupies 212 bytes / 57 instructions:

```text
samp.dll+0x0001d9f0 .. samp.dll+0x0001dac3
entry bytes:
6a ff 68 7b 16 0e 10 64 a1 00 00 00 00 50 64 89
```

The argument is the entry stack slot `[esp+4]`, observed as `[esp+0x12c]`
after the SEH prologue and local allocation. The function ends in a bare
`ret`, so the callback caller owns argument cleanup. The static ABI inference
is therefore:

```text
void __cdecl Handler(RPCParameters *)
```

Ghidra itself leaves the calling convention as `unknown`.

The handler reads the observed `RPCParameters` prefix:

```text
+0x0  payload pointer
+0x4  number of payload bits
```

It constructs a local bitstream at `samp.dll+0x1da29` and makes these reads:

| Call site | Target | Bit count | Local result |
| --- | --- | ---: | --- |
| `samp.dll+0x1da46` | `samp.dll+0x1f9b0` | 16 | actor ID (`uint16`) |
| `samp.dll+0x1da58` | `samp.dll+0x1f9b0` | 32 | facing angle (raw float bits) |

The statically established normal payload is therefore 48 bits. Neither read
return value is tested, and there is no branch requiring a total bit count of
48. Exact behavior for truncated or malformed payloads is `TODO_VERIFY`; an
overlay should delegate such inputs to its original trampoline until that
behavior is measured.

The handler performs no finite, sign, or range check on the decoded angle. At
`samp.dll+0x1da91`, it loads the local angle as a 32-bit integer and pushes
those exact bits into `CActor::SetFacingAngle`.

## ActorPool resolution and guards

The handler uses the same ActorPool chain as RPC 178:

```text
*(samp.dll+0x26eb94) -> NetGame
NetGame+0x3de        -> Pools
Pools+0x10           -> ActorPool
```

There is no null branch for `NetGame` or `Pools`. The first null guard is for
the resulting `ActorPool` pointer.

| Instruction | Condition | Failure destination |
| --- | --- | --- |
| `samp.dll+0x1da6e` | `ActorPool` is nonnull | cleanup `+0x1da9b` |
| `samp.dll+0x1da79` | unsigned actor ID `< 1000` | cleanup `+0x1da9b` |
| `samp.dll+0x1da87` | `ActorPool+0xfa4+id*4` is nonzero | cleanup `+0x1da9b` |
| `samp.dll+0x1da8f` | `ActorPool+0x4+id*4` CActor is nonnull | cleanup `+0x1da9b` |

For a live slot, the call is:

```text
samp.dll+0x1da96 -> samp.dll+0x9c570
```

Every rejection path and the successful call converge at the local-bitstream
cleanup call at `samp.dll+0x1daaa`.

## `CActor::SetFacingAngle`

The downstream method occupies 49 bytes / 18 instructions:

```text
samp.dll+0x0009c570 .. samp.dll+0x0009c5a0
entry bytes:
56 8b f1 8b 46 48 85 c0 74 23 8b 46 44 50 e8 ed
```

It receives `this` in `ecx`, takes the angle at `[esp+4]`, and returns with
`ret 4`. Its static ABI inference is:

```text
void __thiscall CActor::SetFacingAngle(float angle)
```

Ghidra itself leaves the convention as `unknown`.

The exact mutation path is:

1. Return if `CActor+0x48` (cached CPed pointer) is null.
2. Pass `CActor+0x44` to `samp.dll+0xb3b70`; return if the result is null.
3. Pass the original angle bits to `samp.dll+0xb5970`.
4. Store the x87 result as a 32-bit float at `(CActor+0x48)+0x55c`.

The wrapper at `samp.dll+0xb3b70` loads GTA global `0xb74490` and calls GTA
address `0x404910`. `GTA_REVERSED_REF` identifies these as the ped-pool global
and `CPedPool::GetAtRef`, respectively. Thus the `CActor+0x44` handle is
re-resolved and used as a validity guard before the cached `CActor+0x48`
pointer is mutated.

`GTA_REVERSED_REF` identifies `CPed+0x55c` as
`CPed::m_fAimingRotation`; `m_fCurrentRotation` is the preceding field at
`CPed+0x558`. The R5 method writes only the aiming rotation. It does not write
the current rotation and does not issue a GTA script command in this path.

Ghidra finds one direct caller of this method: RPC 175 at
`samp.dll+0x1da96`.

## Angle conversion helper

The helper occupies 102 bytes / 27 instructions:

```text
samp.dll+0x000b5970 .. samp.dll+0x000b59d5
entry bytes:
d9 44 24 04 d8 1d 24 61 0e 10 df e0 f6 c4 41 74
```

It reads a float at `[esp+4]`, returns with a bare `ret`, and leaves its result
in x87 `ST(0)`. `CActor::SetFacingAngle` removes the four-byte argument after
the call. The static ABI inference is:

```text
float __cdecl ConvertActorFacingAngle(float degrees)
```

The five referenced float constants are:

| RVA | Bytes | Float bits | Value |
| --- | --- | --- | ---: |
| `samp.dll+0xe6124` | `00 00 b4 43` | `0x43b40000` | `360.0f` |
| `samp.dll+0xe5940` | `00 00 00 00` | `0x00000000` | `+0.0f` |
| `samp.dll+0xed468` | `00 00 34 43` | `0x43340000` | `180.0f` |
| `samp.dll+0xed47c` | `61 0b b6 3b` | `0x3bb60b61` | approximately `1/180` |
| `samp.dll+0xed480` | `db 0f 49 40` | `0x40490fdb` | approximately pi |

The compare/status-word branches establish:

| Branch | Ordered condition taking the branch | Destination |
| --- | --- | --- |
| `samp.dll+0xb597f` `jz` | input `> 360.0f` | load `+0.0f` |
| `samp.dll+0xb5990` `jnp` | input `< 0.0f` | load `+0.0f` |
| `samp.dll+0xb59a5` `jnz` | input `<= 180.0f` | direct scale path |

For ordered, in-range inputs the exact operation sequence is:

```text
0 <= angle <= 180:
    angle * pi_float * inverse_180_float

180 < angle <= 360:
    -[pi_float - ((angle - 180.0f) * pi_float * inverse_180_float)]
```

These expressions describe the instruction order, not permission to
algebraically fold the operations. Intermediates remain on the x87 stack until
the final `fstp` to `CPed+0x55c`. The active x87 precision and rounding state
can therefore affect exact boundary bits. In particular, a replacement must
not assume that the literal operation sequence at `180.0f` or `360.0f` is
bit-identical to a simplified `angle * (pi / 180)` or `angle -= 360` form.

If an x87 comparison completes with the unordered status (the normal result
when invalid-operation exceptions are masked), the path is also statically
distinguishable:

- NaN does not take either out-of-range zero branch.
- NaN takes the direct scale path and undergoes both multiplications.
- `+inf` takes the `> 360` zero path; `-inf` takes the `< 0` zero path.
- Both signed zeros compare in range and pass through the multiply path.

Consequently, finite values outside `[0, 360]` and both infinities return the
literal positive-zero constant, while NaN is propagated through x87
arithmetic. Exact signaling-NaN and subnormal behavior remains `TODO_VERIFY`.

The later original-client raw trace
[rpc175_set_actor_facing_raw_original_20260723.md](../traces/rpc175_set_actor_facing_raw_original_20260723.md)
corroborated the static path at runtime. It observed `-0 -> -0`,
`180 -> 0x40490fdb`, `360 -> -0`, out-of-range finite values and both
infinities to literal `+0`, and qNaN `0x7fc01234` propagated with its payload
unchanged.

The corresponding replacement trace
[rpc175_set_actor_facing_raw_samp_re_20260723.md](../traces/rpc175_set_actor_facing_raw_samp_re_20260723.md)
replayed the same raw sequence through `samp_re.asi`. All nine immediate
`CPed+0x55c` bit patterns matched with `original_calls=0`; four background
135-degree calls also reproduced the observed PC24 result `0x4016cbe5`.

The later full rebuilt-DLL trace
[rpc175_set_actor_facing_raw_reimpl_20260724.md](../traces/rpc175_set_actor_facing_raw_reimpl_20260724.md)
first exposed nine same-snapshot RPCs collapsing to one deferred physical
write. After adding a bounded ordered raw-event history, the rebuilt DLL
physically applied revisions 49 through 57 exactly once and reproduced all
nine original target bits.

The helper has no outgoing calls. Ghidra finds seven direct call sites:

```text
samp.dll+0x9c58c
samp.dll+0xabf30
samp.dll+0xabf42
samp.dll+0xabf80
samp.dll+0xabf92
samp.dll+0xb0e74
samp.dll+0xb0e86
```

## Progressive hook contract

The smallest whole-instruction RPC-handler patch span suitable for a five-byte
jump is seven bytes:

```text
samp.dll+0x1d9f0:
6a ff                         push -1
68 7b 16 0e 10                push 0x100e167b
continuation: samp.dll+0x1d9f7
```

The second instruction contains a PE-relocated preferred-base address. A
runtime preflight must validate `module_base+0xe167b`, rather than requiring
the preferred-base literal bytes. A trampoline must replay both complete
instructions before continuing at `samp.dll+0x1d9f7`.

The downstream method has a six-byte whole-instruction patch span:

```text
samp.dll+0x9c570: 56 8b f1 8b 46 48
continuation: samp.dll+0x9c576
```

The conversion helper requires ten bytes to preserve whole instructions:

```text
samp.dll+0xb5970: d9 44 24 04 d8 1d 24 61 0e 10
continuation: samp.dll+0xb597a
```

Hooking the helper additionally requires preserving its x87 input/result
contract. No hook was installed by this inspection. Patch ownership,
instruction-cache flush, coexistence with other ASIs, exact identity
preflight, and conditional restoration remain overlay requirements.

## Open verification points

- `TODO_VERIFY`: truncated/malformed RPC payload behavior; neither bit-read
  result is checked.
- `OBSERVED_037 + PROBE_TRACE`: original readback bits at `CPed+0x55c` are
  documented for `-0`, `+0`, `180`, `360`, representative out-of-range
  finite values, both infinities, and one qNaN payload.
- `TODO_VERIFY`: values immediately outside the boundaries, signaling NaNs,
  subnormals, and alternate x87 control words.
- `TODO_VERIFY`: no-op behavior for inactive actor slots, null CActor/CPed,
  and a stale `CActor+0x44` GTA ped reference.
- `TODO_VERIFY`: visual/current-rotation timing after only
  `m_fAimingRotation` is written.
