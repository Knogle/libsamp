# ReLoop RPC and GTA-opcode parity contract

## Scope

Compatibility runs require more than the same final server-visible state.
For every tested server operation the replacement must:

1. decode the same SA-MP 0.3.7 RPC/packet and payload shape;
2. enter the equivalent original handler lifecycle;
3. use the same GTA script opcode when static analysis or an original probe
   shows that the R5 handler uses one;
4. preserve the original ordering relative to sync emission and callbacks.

Direct GTA writes or helper calls may remain guarded fallbacks, but they are
`INFERRED,TODO_VERIFY` and cannot establish parity unless original R5 uses the
same path. A matching state-only matrix is therefore reported as
`OPCODE_PARITY_UNVERIFIED`, not as a full compatibility pass.

## Evidence levels

- `STATIC_037`: original handler RVA, DLL SHA-256, relevant whole-instruction
  bytes, and the GTA script opcode/call path are recorded.
- `PROBE_TRACE`: the original handler/opcode is observed during the same bare
  server scenario and correlated with the request marker.
- `STATE_ONLY`: original and replacement server event/status signatures match,
  but handler/opcode equivalence is not yet proven.
- `TODO_VERIFY`: no full-parity verdict is allowed.

The original DLL currently used by the Legacy prefix has SHA-256
`b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`.

## First automated finding

`PROBE_TRACE` from replacement run `20260718-214001-replacement-all-420531`
shows RPC 19 reaching the sequenced facing-angle bridge and GTA opcode `0173`
with 90 degrees after the vehicle test, while open.mp later observed facing 0.
The original run `20260718-213514-original-all-420531` observed facing 90.
Targeted run `20260718-215252-replacement-player-432931` also proved that
flipping the sender's quaternion Z sign makes open.mp observe 270 degrees, so
the 0.3.7 negative-half-angle convention is retained. The open mismatch is
currently localized to state/order left by the preceding vehicle transition;
the isolated RPC/opcode path must be re-run after the sign revert.

The follow-up isolated run `20260718-215644-replacement-player-434388` passed
with the original negative-half-angle convention. Full original run
`20260718-220442-original-all-436912` later reported facing 0.027 after the
vehicle transition, proving that delayed server-facing state is overwritten by
normal client sync even on R5. The fixture therefore treats facing as an
RPC-19/opcode-0173 trace requirement rather than a delayed hard state check.

## Next trace work

Build a scenario manifest mapping each `test_cmds` event to its expected RPC,
original handler RVA and GTA opcode(s). Normalize original probe events and
replacement `samp_net_trace.log`/`samp_runtime.log` events into the same tuple:

```text
request, scenario_event, rpc_id, handler_rva_or_name, gta_opcode, order
```

The matrix comparison can change `opcode_path_verdict` from `TODO_VERIFY` only
when all required tuples for the selected group match and carry either
`STATIC_037` or `PROBE_TRACE` evidence.
