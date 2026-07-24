# RPC 176 ScrSetActorPos / SetActorPos: focused R5 static inspection

## Scope and identity

This note is a bounded static inspection of the original SA-MP 0.3.7-R5
RPC 176 handler, the shared SA-MP entity-position method reached by the
handler, and the two BitStream helpers needed to establish its malformed-length
behavior. It contains no decompiler pseudocode.

```text
samp.dll SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
samp.dll MD5=5ba5f0be7af99dfd03fb39e88a970a2b
format=PE32 i386
preferred image base=0x10000000
Ghidra language=x86:LE:32:default
Ghidra compiler spec=windows
```

Evidence in this note is `STATIC_037` unless another evidence tag is given.
The downstream GTA interpretation is explicitly tagged
`GTA_REVERSED_REF`/`PROBE_TRACE`.

## Reproducible artifacts

The focused script is
[`tools/ghidra/InspectActorSetPosition.java`](../../tools/ghidra/InspectActorSetPosition.java).
It emits identity, function, instruction, call, branch, data-reference,
scalar, incoming-reference, and bounded raw-memory TSV files under:

```text
analysis/generated/ghidra_rpc176_20260723/
```

Reproduce the final run after creating the temporary parent directory:

```bash
tools/ghidra/analyze-headless.sh /tmp/ghidra-rpc176 rpc176-r5 \
  -import '/absolute/path/to/original-r5/samp.dll' \
  -scriptPath '/absolute/path/to/repository/tools/ghidra' \
  -postScript InspectActorSetPosition.java \
    '/absolute/path/to/repository/analysis/generated/ghidra_rpc176_20260723' \
  -deleteProject
```

The verification run completed import, analysis, the focused post-script, and
post-analysis successfully with Ghidra 12.1.2. Ghidra emitted its unrelated
optional `WindowsResourceReference.java` analyzer warning; the warning did not
abort analysis or the post-script. `-deleteProject` removed the temporary
project after export.

## RPC registration, ABI, and payload

The registration sequence at `samp.dll+0x1e71e` loads the RakClient vtable,
pushes handler `samp.dll+0x1dad0`, pushes the address of the identifier at
`samp.dll+0xe625c`, and calls vtable slot `+0x58`. The identifier bytes are:

```text
samp.dll+0xe625c: b0 00 00 00
```

This directly establishes the registration as RPC 176 (`0xb0`).

The handler occupies 258 bytes / 73 instructions:

```text
samp.dll+0x0001dad0 .. samp.dll+0x0001dbd1
entry bytes:
6a ff 68 9b 16 0e 10 64 a1 00 00 00 00 50 64 89
```

It obtains its argument from the entry stack slot `[esp+4]`, observed as
`[esp+0x134]` after the SEH prologue and local allocation. It ends in a bare
`ret`, so the callback caller owns argument cleanup. The static ABI inference
is:

```text
void __cdecl Handler(RPCParameters *)
```

Ghidra itself leaves the calling convention as `unknown`.

The handler reads the observed `RPCParameters` prefix:

```text
+0x0  payload pointer
+0x4  number of payload bits (signed 32-bit use in this handler)
```

It constructs a non-owning local BitStream at `samp.dll+0x1db09`, then invokes
`samp.dll+0x1f9b0` in this order:

| Call site | Bit count | Local result |
| --- | ---: | --- |
| `samp.dll+0x1db26` | 16 | actor ID (`uint16`) |
| `samp.dll+0x1db38` | 32 | X (raw float bits) |
| `samp.dll+0x1db4a` | 32 | Y (raw float bits) |
| `samp.dll+0x1db5c` | 32 | Z (raw float bits) |

The normal payload is therefore 112 bits. The three coordinate words are
passed to the downstream method without finite, range, or canonicalization
checks. Payload bits after the first 112 are never consumed.

## Length and failed-read behavior

The handler does not compare `numberOfBitsOfData` with 112 and does not test
any of the four BitStream read return values.

For a nonnegative advertised bit count `n`, the instructions at
`samp.dll+0x1daf7..+0x1db03` pass this byte length to the non-owning BitStream
constructor:

```text
bytes = floor(n / 8) + 1
internal readable capacity = bytes * 8 bits
```

Thus even a normal 112-bit packet is represented internally as 120 readable
bits. Within the ordinary non-overflowing range, and based solely on this
internal capacity check, all four reads can succeed for advertised lengths
from 104 bits upward. This does **not** prove that reading beyond the
transport-declared allocation is safe.

The focused export of `samp.dll+0x1f9b0` establishes that a read fails before
zeroing its destination or advancing the read offset when either the requested
bit count is nonpositive or `offset + requested > internal capacity`. The RPC
handler's four destination locals are not initialized before the calls.
Consequently, a truncated packet can leave one or more indeterminate stack
values and still proceed through ActorPool resolution. Negative advertised
lengths additionally pass through signed arithmetic and are not rejected.

This unsafe malformed-input behavior is `STATIC_037`, but exact runtime
outcomes are deliberately `TODO_VERIFY`: a compatibility overlay should not
perform an out-of-bounds read merely to imitate it. A conservative replacement
can require 112 readable bits and delegate shorter/malformed input to the
original trampoline while that path is still available.

## ActorPool resolution and guards

All four reads occur before the pool lookup. The handler then follows:

```text
*(samp.dll+0x26eb94) -> NetGame
NetGame+0x3de        -> Pools
Pools+0x10           -> ActorPool
```

There is no null branch for `NetGame` or `Pools`; the first null guard applies
to the resulting `ActorPool`.

| Instruction | Condition | Failure destination |
| --- | --- | --- |
| `samp.dll+0x1db72` | `ActorPool` is nonnull | cleanup `+0x1dba9` |
| `samp.dll+0x1db7d` | unsigned actor ID `< 1000` | cleanup `+0x1dba9` |
| `samp.dll+0x1db8b` | `ActorPool+0xfa4+id*4` is nonzero | cleanup `+0x1dba9` |
| `samp.dll+0x1db93` | `ActorPool+0x4+id*4` wrapper is nonnull | cleanup `+0x1dba9` |

For a live slot, the downstream call is:

```text
samp.dll+0x1dba4 -> samp.dll+0x9f040
```

All rejected branches and the successful call converge at local-BitStream
cleanup `samp.dll+0x1dba9`.

## Position method and exact dispatch

The method at `samp.dll+0x9f040` is shared: Ghidra finds 25 direct callers, one
of which is RPC 176. It occupies 115 bytes / 41 instructions:

```text
samp.dll+0x0009f040 .. samp.dll+0x0009f0b2
entry bytes:
55 8b ec 51 8b 41 40 85 c0 89 45 fc 74 5f 81 38
```

It receives `this` in `ecx`, X/Y/Z at `[ebp+8/+0xc/+0x10]`, and returns with
`ret 0xc`. The static ABI inference is:

```text
void __thiscall SetPosition(float x, float y, float z)
```

Ghidra itself leaves the convention as `unknown`.

The method first obtains the GTA entity pointer at wrapper `+0x40`. It returns
without side effects if that pointer is null or if its first word is the
literal vtable address `0x00863c40`. `GTA_REVERSED_REF` identifies the latter
as the base `CPlaceable` vtable, whose position virtuals are not a valid
derived-entity target.

It then reads the entity model ID at `entity+0x22` and selects one of two
paths:

| Model IDs | Static dispatch |
| --- | --- |
| all except 538, 537, 449 | call `entity->vtable[0x38/4]` with raw X/Y/Z and final boolean `false` |
| 538, 537, 449 | call the SA-MP script-command bridge at `samp.dll+0xb2310` |

`GTA_REVERSED_REF` identifies the three special IDs as `STREAK`, `FREIGHT`,
and `TRAM`. For this branch, the method:

1. reads the GTA handle at wrapper `+0x44`;
2. promotes each input float to a C varargs `double`;
3. passes handle, X, Y, and Z with the descriptor at `samp.dll+0xec50c`.

The bounded descriptor is:

```text
c7 07 69 66 66 66 00 00
```

This is SCM opcode `0x07c7` with format `ifff`.
`GTA_REVERSED_REF` names it `COMMAND_SET_MISSION_TRAIN_COORDINATES`. The
method performs no independent handle, finite-value, or coordinate-range
validation before dispatch. Exact GTA-internal train side effects remain
`TODO_VERIFY`.

For a normal Actor entity, prior `PROBE_TRACE` resolved virtual slot `+0x38`
to `gta_sa.exe+0x5e4110`, `CPed::Teleport`. The pinned GTA identity for that
trace is:

```text
gta_sa.exe SHA256=a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26
```

`GTA_REVERSED_REF` describes that target as conditionally flushing ped tasks,
removing the ped from `CWorld`, setting its position, clearing standing and
damage-reference state, adding it back to `CWorld`, and zeroing move and turn
speed. The R5 SA-MP wrapper itself makes no coordinate-field write: its normal
side effect is wholly delegated to the virtual call. The final boolean is
always zero.

## Progressive hook contract

The smallest whole-instruction RPC-handler patch span suitable for a five-byte
jump is seven bytes:

```text
samp.dll+0x1dad0:
6a ff                         push -1
68 9b 16 0e 10                push 0x100e169b
continuation: samp.dll+0x1dad7
```

The second instruction contains a PE-relocated preferred-base address. Runtime
preflight must validate `module_base+0xe169b`; a trampoline must replay both
complete instructions before continuing at `samp.dll+0x1dad7`.

The downstream method has this independent seven-byte whole-instruction span:

```text
samp.dll+0x9f040:
55                            push ebp
8b ec                         mov ebp, esp
51                            push ecx
8b 41 40                      mov eax, [ecx+0x40]
continuation: samp.dll+0x9f047
```

The normal-path indirect call at `samp.dll+0x9f07c` is only three bytes and is
not itself a five-byte inline-hook span. No hook was installed by this
inspection. Patch ownership, byte validation, instruction-cache flush,
coexistence with other ASIs, and restoration remain overlay requirements.

## Open verification points

- `TODO_VERIFY`: original runtime behavior for declared lengths below 112,
  especially the internally accepted 104..111 range; replacements should stay
  memory-safe.
- `TODO_VERIFY`: raw coordinate edge vectors (`-0`, infinities, NaNs, large
  finite values) and immediate physical readback through `CPed::Teleport`.
- `TODO_VERIFY`: missing `NetGame`/`Pools` is statically unguarded but should
  not be induced in a normal runtime probe.
- `TODO_VERIFY`: special train-model dispatch and exact
  `COMMAND_SET_MISSION_TRAIN_COORDINATES` side effects.
