# Legacy gang-zone RPC compatibility

Date: 2026-07-16

## Evidence

Original client under analysis:

* SA-MP 0.3.7-R5 `samp.dll`
* SHA-256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
* Ghidra inventory:
  `analysis/generated/ghidra_samp_20260609/functions.tsv`

`STATIC_037` identifies these handlers and pool methods:

| Behavior | RPC | Original RVA |
| --- | ---: | ---: |
| Add/show zone | 108 | `samp.dll+0x1D080` |
| Remove/hide zone | 120 | `samp.dll+0x1D1A0` |
| Start flashing | 121 | `samp.dll+0x1D250` |
| Stop flashing | 85 | `samp.dll+0x1D310` |
| Pool create | - | `samp.dll+0x002180` |
| Pool flash | - | `samp.dll+0x002200` |
| Pool stop-flash | - | `samp.dll+0x002220` |
| Pool delete | - | `samp.dll+0x002240` |
| Pool draw | - | `samp.dll+0x002270` |

The layouts recovered from those handlers are:

| RPC | Wire layout | Minimum bytes |
| ---: | --- | ---: |
| 108 | `u16 zone + f32 minX + f32 minY + f32 maxX + f32 maxY + u32 color` | 22 |
| 120 | `u16 zone` | 2 |
| 121 | `u16 zone + u32 alternateColor` | 6 |
| 85 | `u16 zone` | 2 |

The local legacy source cross-check in
`local_reference_sources/saco/net/scriptrpc.cpp` and
`local_reference_sources/saco/net/gangzonepool.cpp` agrees with the static
layouts and pool behavior. It stores drawing bounds in the GTA-specific order
`{minX, maxY, maxX, minY}`, initializes base and alternate colors to the wire
color, and toggles all zones on one global 500 ms phase.

## Original render path

GTA target: 1.0 US `gta_sa.exe`, SHA-256
`a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`.

`STATIC_037` shows that R5 installs a call hook for both radar and pause-map
rendering:

| GTA call site | Original bytes | Replacement behavior |
| ---: | --- | --- |
| `gta_sa.exe+0x1869BF` (`0x5869BF`) | `E8 8C FC FF FF` | call SA-MP zone-overlay thunk |
| `gta_sa.exe+0x1759E4` (`0x5759E4`) | `E8 67 0C 01 00` | call SA-MP zone-overlay thunk |

Both original calls target GTA `gta_sa.exe+0x186650` (`0x586650`). The R5
wrapper at `samp.dll+0x0A1440` ultimately calls GTA
`gta_sa.exe+0x1853D0` (`0x5853D0`, `bounds, &color, menuFlag`), where
`menuFlag` is read from GTA 1.0 US data RVA `gta_sa.exe+0x7A67A4`
(`0xBA67A4`).

The replacement therefore uses the two validated call sites rather than a
generic EndScene overlay. Installation must compare all five original bytes,
flush the instruction cache, retain the original bytes, and restore them on
shutdown. This keeps both the radar and pause-map paths covered.

## Replacement design

The RakNet adapter now keeps:

* a bounded 128-entry ordered event ring;
* an authoritative 1024-slot state snapshot for recovery if the ring wraps;
* finite/range checks for coordinates and a strict `zone < 1024` check;
* base and alternate colors without an extra RGBA/ABGR conversion.

Runtime semantics mirror the original pool:

* RPC 108 replaces a slot and initializes both colors;
* RPC 120 clears a slot;
* RPC 121 changes only the alternate color of an existing slot;
* RPC 85 copies the base color back to the alternate color;
* all active slots share one 500 ms flash phase;
* connect reset, game-mode restart, disconnect, and shutdown clear the pool.

## Bare fixture

The bare gamemode exposes a visible four-zone batch around the fixed Area 51
spawn:

* `/rpczones` starts a repeating four-phase test that exercises RPCs
  108, 121, 85, and 120;
* `/rpczonesoff` stops flashing and hides all four slots;
* the result can be inspected on the HUD radar and the ESC pause map.

The fixture follows the official open.mp GangZone Create/Show/Flash/
StopFlash/Hide/Destroy APIs. The Pawn script is compile-checked only; no server
smoke test was run by request.

## Verification status

* `STATIC_037`: handler RVAs, pool size/order/timing, hook sites, original
  bytes, GTA wrapper call.
* `OPENMP_REF`: bare-server GangZone native semantics.
* `TODO_VERIFY`: replacement runtime run, radar/pause-map visual parity, exact
  alpha/color parity, and hook restoration during a real shutdown.
