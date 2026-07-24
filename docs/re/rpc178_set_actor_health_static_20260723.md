# RPC 178 SetActorHealth: focused R5 static inspection

## Scope and identity

This note is a bounded static inspection of the original SA-MP 0.3.7-R5
RPC 178 handler and its immediate `CActor::SetHealth` target. It does not
contain decompiler pseudocode.

```text
samp.dll SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
samp.dll MD5=5ba5f0be7af99dfd03fb39e88a970a2b
format=PE32 i386
preferred image base=0x10000000
Ghidra language=x86:LE:32:default
Ghidra compiler spec=windows
```

Evidence in this note is `STATIC_037` unless a statement is explicitly marked
otherwise. Runtime behavior still requires a comparable original-client probe.

## Reproducible artifacts

The focused script is
[`tools/ghidra/InspectActorSetHealth.java`](../../tools/ghidra/InspectActorSetHealth.java).
It emits identity, function, instruction, call, branch, data-reference, scalar,
incoming-reference, and three bounded data-window TSV files under:

```text
analysis/generated/ghidra_rpc178_20260723/
```

Reproduce it with an existing temporary parent directory:

```bash
tools/ghidra/analyze-headless.sh /tmp/ghidra-rpc178 rpc178-r5 \
  -import /absolute/path/to/samp.dll \
  -scriptPath /absolute/path/to/tools/ghidra \
  -postScript InspectActorSetHealth.java \
    /absolute/path/to/analysis/generated/ghidra_rpc178_20260723 \
  -deleteProject
```

The final verification run completed the import, analysis, focused script, and
post-analysis successfully with Ghidra 12.1.2. Ghidra emitted its unrelated
optional `WindowsResourceReference.java` analyzer warning; it did not abort
the analysis or the post-script.

## RPC handler ABI and payload path

The RPC 178 handler occupies 212 bytes / 57 instructions:

```text
samp.dll+0x0001dbe0 .. samp.dll+0x0001dcb3
entry bytes:
6a ff 68 bb 16 0e 10 64 a1 00 00 00 00 50 64 89
```

The argument is read from the entry stack slot `[esp+4]` after accounting for
the SEH prologue and `0x11c` local allocation. The handler ends in a bare
`ret`, so the caller owns argument cleanup. `void __cdecl
Handler(RPCParameters*)` is therefore the static ABI inference; Ghidra itself
leaves the convention as `unknown`.

The handler reads these `RPCParameters` fields:

```text
+0x0  payload pointer
+0x4  number of payload bits
```

It constructs a local bitstream with `samp.dll+0x1f840`, then calls
`samp.dll+0x1f9b0` twice:

| Call site | Bit count | Local result |
| --- | ---: | --- |
| `samp.dll+0x1dc36` | 16 | actor ID (`uint16`) |
| `samp.dll+0x1dc48` | 32 | health (raw float bits) |

The statically proven normal input is therefore 48 bits. The handler does not
compare the total bit count with 48 and does not branch on either bit-read
return value. Exact behavior for truncated or otherwise malformed payloads is
`TODO_VERIFY`; a progressive replacement should delegate such inputs to the
original trampoline until that behavior is measured.

## Pool resolution and guards

After decoding, the handler follows this exact chain:

```text
*(samp.dll+0x26eb94) -> NetGame
NetGame+0x3de        -> Pools
Pools+0x10           -> ActorPool
```

There is no null branch for the first two dereferences. The first null guard is
for `ActorPool` at `samp.dll+0x1dc5c`.

The remaining branches are:

| Instruction | Condition | Failure destination |
| --- | --- | --- |
| `samp.dll+0x1dc64` | unsigned actor ID `< 1000` | cleanup `+0x1dc8b` |
| `samp.dll+0x1dc6e` | `ActorPool+0xfa4+id*4` is nonzero | cleanup `+0x1dc8b` |
| `samp.dll+0x1dc79` | `ActorPool+0x4+id*4` CActor is nonnull | cleanup `+0x1dc8b` |

For a live slot, the raw 32-bit health value is pushed without finite/range
validation and the handler directly calls:

```text
samp.dll+0x1dc86 -> samp.dll+0x9c5d0
```

All rejected normal branches converge at the local-bitstream cleanup at
`samp.dll+0x1dc8b`.

## `CActor::SetHealth`

The downstream method occupies 59 bytes / 17 instructions:

```text
samp.dll+0x0009c5d0 .. samp.dll+0x0009c60a
entry bytes:
8b 41 48 85 c0 74 31 8b 54 24 04 89 90 40 05 00
```

It receives `this` in `ecx`, reads the float bits from `[esp+4]`, and returns
with `ret 4`. `void __thiscall CActor::SetHealth(float)` is therefore the
static ABI inference; Ghidra itself leaves the convention as `unknown`.

The method:

1. returns if `CActor+0x48` (CPed) is null;
2. copies the raw health bits to `CPed+0x540`;
3. compares the stored float with `0.0f` from `samp.dll+0xe5940`;
4. for an ordered value less than or equal to zero, calls
   `samp.dll+0xb2310` with the GTA actor handle from `CActor+0x44` and the
   descriptor at `samp.dll+0xec2a8`.

The bounded descriptor bytes are:

```text
21 03 69 00 00 00 00 00
```

The comparison-condition interpretation in step 4 is
`STATIC_037 + INFERRED` from `fcomp`, `fnstsw`, `test ah,0x41`, and the
following `jp`. Its runtime side effect and the semantic name of the
`samp.dll+0xb2310` helper remain `TODO_VERIFY`. The raw CPed health write itself
is directly established by the instruction sequence.

Ghidra finds two direct callers of this method: ActorPool creation at
`samp.dll+0x19c3` and RPC 178 at `samp.dll+0x1dc86`.

## Progressive hook contract

The smallest whole-instruction RPC-handler patch span is seven bytes:

```text
samp.dll+0x1dbe0:
6a ff                         push -1
68 bb 16 0e 10                push 0x100e16bb
continuation: samp.dll+0x1dbe7
```

The immediate in the second instruction is PE-relocated. A runtime preflight
must validate `module_base+0xe16bb`, not assume preferred-base literal bytes.
A trampoline must replay both complete instructions before continuing at
`samp.dll+0x1dbe7`.

The downstream method can be separately preflighted with:

```text
samp.dll+0x9c5d0: 8b 41 48 85 c0
```

No hook was installed by this inspection. Patch ownership, instruction-cache
flush, coexistence with other ASIs, and conditional restoration remain
requirements of the overlay implementation.

## Open verification points

- `TODO_VERIFY`: malformed/truncated RPC payload behavior, since the handler
  ignores bit-read return values.
- `TODO_VERIFY`: runtime call convention observation in a clean original run,
  although stack cleanup strongly establishes the static ABI inference.
- `TODO_VERIFY`: exact runtime side effect of the nonpositive-health
  `samp.dll+0xb2310` call.
- `TODO_VERIFY`: original and overlay traces for valid 48-bit actor-health
  updates, including missing pool, inactive slot, missing CActor, positive,
  zero, negative, and NaN health cases.
