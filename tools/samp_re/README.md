# `samp_re` progressive overlay ASI

`samp_re.asi` is the conservative, per-function replacement layer for the
original SA-MP 0.3.7-R5 client. The currently supported handlers are RPC 175
(`ScrSetActorFacingAngle`) and RPC 178 (`ScrSetActorHealth`). Loading the ASI
is passive by default.

The active configuration is sampled by the worker thread at process start:

```text
SAMP_RE_MODE=bypass|shadow|replace
SAMP_RE_FUNCTIONS=rpc175_set_actor_facing_angle,rpc178_set_actor_health
SAMP_RE_DISABLE=1
```

- `bypass` (and an absent or invalid mode) installs no hook.
- `shadow` installs each selected verified hook and invokes the corresponding
  original handler exactly once. RPC175 logs raw payload bits plus
  post-original `CPed+0x55c` readback; RPC178 keeps its existing raw-payload
  classification log.
- `replace` handles RPC175 payloads of at least 48 bits (R5 consumes the first
  48) and exactly 48-bit RPC178 payloads through the statically verified
  ActorPool path. RPC175 reproduces the R5 x87
  degrees-to-heading conversion and writes `CPed+0x55c` after the original
  `CActor+0x48`/GTA-reference checks. RPC178 uses the original
  `CActor::SetHealth` downstream. Malformed or unreadable payloads invoke the
  corresponding original handler exactly once.
- `SAMP_RE_DISABLE=1` or `samp_re_disabled.flag` beside the ASI is a kill
  switch. Before installation it prevents all patching. During an active run
  it atomically switches every installed hook to original-trampoline-only
  behavior. The seven code bytes at each selected entry remain installed
  until process teardown; this avoids a non-atomic live x86 unpatch while
  another thread could fetch a handler entry.

`shadow` and `replace` additionally require the exact function token in
`SAMP_RE_FUNCTIONS`; no token means no patch. The overlay refuses to install
while either `samp_probe_actor_hooks.flag` or
`samp_probe_actor_heavy.flag` exists beside it, or while the equivalent probe
environment switches are enabled. The focused Actor probe and this overlay
must therefore run in separate evidence captures.

Hook map (`STATIC_037`, original R5 SHA below):

| Function | Module + RVA | Original seven bytes | Installed bytes |
| --- | --- | --- | --- |
| RPC175 | `samp.dll+0x0001d9f0` | `6a ff 68 <base+0x000e167b>` | `e9 <rel32> 90 90` |
| RPC178 | `samp.dll+0x0001dbe0` | `6a ff 68 <base+0x000e16bb>` | `e9 <rel32> 90 90` |

Both patches are seven bytes. Failed pre-publication writes restore the saved
bytes and page protection. Once published, the kill switch retains owned hook
bytes and routes calls through the corresponding original trampoline; physical
restoration is left to process teardown to avoid a live x86 unpatch race.

Every active install requires all of the following:

- the loaded `samp.dll` file hashes to
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`;
- the exact R5 PE timestamp, entry RVA, preferred base, image size, machine,
  and section count match;
- every selected relocated seven-byte RPC prologue matches;
- RPC175 additionally matches the `CActor::SetFacingAngle`, GTA ped-reference
  wrapper, conversion-helper prologues, and all five conversion constants;
- RPC178 additionally matches the `CActor::SetHealth` prologue;
- no other entry hook already owns a selected RPC.

Logs go to `SAMPDLL_LOG_DIR\samp_re.log` when that directory is supplied,
otherwise beside `samp_re.asi`. An active mode refuses to proceed if neither
location can be opened. Logging uses Win32 append I/O rather than a persistent
CRT stream so it remains usable in the native Windows lab setup.

Build and test:

```bash
cmake -S tools/samp_re -B /tmp/samp-re-test -G Ninja
cmake --build /tmp/samp-re-test
ctest --test-dir /tmp/samp-re-test --output-on-failure

tools/samp_re/build_win32.sh
```

RPC175 tests cover raw decode and the original raw golden results for `+0`,
`-0`, `180`, `360`, `-45`, `540`, `+inf`, `-inf`, and qNaN, plus every
truncated bit/byte length and acceptance of trailing bits. They set x87
precision-control to GTA's observed PC24 state and include the discriminating
original mapping `135` -> `0x4016cbe5`. The production i686 converter emits
the original register-only x87 arithmetic sequence and inherits the game's
active control word. The golden input to `CPed+0x55c` mapping is documented in
[`docs/traces/rpc175_set_actor_facing_raw_original_20260723.md`](../../docs/traces/rpc175_set_actor_facing_raw_original_20260723.md).
The native Windows replacement replay matches all nine raw mappings with
immediate field readback and `original_calls=0`; see
[`docs/traces/rpc175_set_actor_facing_raw_samp_re_20260723.md`](../../docs/traces/rpc175_set_actor_facing_raw_samp_re_20260723.md).

Native Windows RPC178 shadow and replace runs match the original valid
positive Actor fixture, including raw CPed-health readback and live
kill-switch passthrough. See
[`docs/traces/rpc178_samp_re_parity_20260723.md`](../../docs/traces/rpc178_samp_re_parity_20260723.md).
