# RPC178 `samp_re` shadow/replace parity (2026-07-23)

## Scope and identity

This trace compares the original SA-MP 0.3.7-R5 RPC178 handler with the first
progressive `samp_re.asi` replacement on the native Windows lab.

```text
samp.dll SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
gta_sa.exe SHA256=a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26
samp_re.asi SHA256=c17791b13a28799fdfa9204528bf26fb06ed346b4cf56fe036419716bed7ee2e
server=192.168.3.181:7778
probe profile=passive
```

The original baseline is
[rpc178_set_actor_health_original_20260723.md](rpc178_set_actor_health_original_20260723.md).
The static handler and downstream-method contract is
[rpc178_set_actor_health_static_20260723.md](../re/rpc178_set_actor_health_static_20260723.md).

This result covers valid, exactly 48-bit RPC178 payloads for four live ActorPool
slots with positive health. Malformed inputs and the remaining original
no-op/side-effect branches are not generalized from this trace.

## Shadow run

```text
run=20260723_193838_rpc178_shadow_actor_cycle_24d925ba
nickname=ShadowActor
samp_re.log SHA256=0fe0384e065c3f7a0045e2d08e0b327561fd7c13b98c44b8ad7d68014dc124c6
samp_probe.log SHA256=bf1f4af7cb22588ce45b044a7cbc4bb7796ad0870192a5b6484353a3a5697386
```

`PROBE_TRACE`: the runtime accepted the R5 file hash, PE identity, relocated
seven-byte RPC178 prologue, and downstream `CActor::SetHealth` prologue before
installing the shadow hook. The first group was:

| Sequence | Actor | Health bits | Decode | Original calls |
| ---: | ---: | ---: | --- | ---: |
| 1 | 0 | `0x42c80000` | exact 48 bits | 1 |
| 2 | 1 | `0x42aa0000` | exact 48 bits | 1 |
| 3 | 2 | `0x428c0000` | exact 48 bits | 1 |
| 4 | 3 | `0x425c0000` | exact 48 bits | 1 |

The identical group repeated as sequences 5-8. No fallback,
missing-trampoline, ownership-loss, exception, or dump marker occurred.

## Replace run

```text
run=20260723_194451_rpc178_replace_actor_cycle_00f85b47
nickname=ReplaceActor
samp_re.log SHA256=2a854d20f0726a5a864fcf3f3e22908a04b2ff582700b303e21ed1859edb9581
samp_probe.log SHA256=30181ad62b69640b3bdda6ee593da31b0031866cd5fb71224fd1f9858b2c2748
```

`PROBE_TRACE`: the same runtime gates accepted at a different relocated
`samp.dll` base. Sixteen RPC178 calls across repeated create/destroy cycles
were handled by the replacement:

- all 16 logged `result=applied`;
- all 16 logged `original_calls=0`;
- all 16 read `CActor+0x48 -> CPed`, then read the raw value at
  `CPed+0x540`;
- all 16 logged `readback=1` and `health_match=1`;
- no call logged fallback, no-op, unreadable state, missing downstream,
  ownership loss, or a mismatched readback.

The first group exactly matched the original and shadow payloads:

| Sequence | Actor | Input bits | Stored CPed bits | Original calls |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 0 | `0x42c80000` | `0x42c80000` | 0 |
| 2 | 1 | `0x42aa0000` | `0x42aa0000` | 0 |
| 3 | 2 | `0x428c0000` | `0x428c0000` | 0 |
| 4 | 3 | `0x425c0000` | `0x425c0000` | 0 |

`PROBE_TRACE + STATIC_037`: this establishes the valid positive-health path:
the overlay decode, ActorPool lookup, x86 `thiscall`, and original downstream
method produce the same raw health storage while suppressing the original RPC
handler.

A timed screenshot
`C:\samp-test\screenshots\20260723_195051_350_replace_phase2_visible.png`
(SHA256
`d267999ecc7939efc7b87a6cb50cb20c1497dc67e946b77de830a42435cff318`)
showed all four fixture actors rendered around the player after the health
phase.

## Live kill switch and shutdown

`PROBE_TRACE`: with no active callback, creating the fixed
`samp_re_disabled.flag` changed the installed hook once to live bypass. The
overlay retained its entry bytes and trampoline instead of performing a
non-atomic live unpatch. The next four RPC178 calls logged state 2 and
`original_calls=1`; no replacement call occurred after the transition.

Both shadow and replace GTA processes remained responsive and exited normally
through `/q`. Their passive probe logs reached one successful `closesocket`
and one successful `WSACleanup`, with no `exception_filter` record or dump.
The remaining SA-MP browser was stopped only after GTA had exited. The managed
kill-switch flag was then removed and the lab overlay profile returned to
`bypass`.

## Remaining verification

- `TODO_VERIFY`: truncated, oversized, and otherwise malformed RPC178 input.
- `TODO_VERIFY`: ActorPool null, actor ID 1000+, inactive slot, and null actor
  branches.
- `TODO_VERIFY`: positive infinity, NaN, zero, and negative health, including
  the original nonpositive helper at `samp.dll+0x000b2310`.
- `TODO_VERIFY`: first-install thread quiescence under stress. The lab installs
  before connection/RPC traffic, but the seven-byte x86 entry write is not
  atomic.
