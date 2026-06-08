# SA-MP 0.3.7-R5 Golden Trace - 2026-06-05

## Scenario

- Original `samp.dll` in the GTA prefix.
- ASI probe build: safe mode with file hooks disabled by default.
- User flow: launch, connect to local open.mp server, dialog/freeroam/spawn interaction, shutdown.

## Evidence

- `OBSERVED_037` original DLL SHA256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- `PROBE_TRACE` safe run:
  `original_safe_probe.log`
- `PROBE_TRACE` rejected exploratory file-hook run:
  `original_crash_probe_filehook_spam.log`

## Safe Run Findings

- `OBSERVED_037 + PROBE_TRACE`: original module loaded at `0x02530000`, image size `0x0027e000`.
- `OBSERVED_037 + PROBE_TRACE`: GTA entry gate sequence observed as `5 -> 8 -> 9`.
- `OBSERVED_037 + PROBE_TRACE`: ped and vehicle pools become non-null after entry gate reaches `8`.
- `OBSERVED_037 + PROBE_TRACE`: original patches `0x0052cf10` from `0x56` to `0xc3`, disabling normal time pass.
- `OBSERVED_037 + PROBE_TRACE`: original redirects the graphics loop target from `0x0053e230` to `samp.dll+0x000cf4ff0` during early init.
- `OBSERVED_037 + PROBE_TRACE`: networking thread starts at `samp.dll+0x000f8d53`.
- `OBSERVED_037 + PROBE_TRACE`: socket send callsite observed at `samp.dll+0x00053b19`; socket receive callsite observed at `samp.dll+0x00053a57`.
- `OBSERVED_037 + PROBE_TRACE`: first outbound packet is 4 bytes, `08 1e 77 da`; first server response observed is 3 bytes, `1a fa 55`.
- `OBSERVED_037 + PROBE_TRACE`: periodic sync packets of 78 bytes appear after spawn/gameplay.
- `OBSERVED_037 + PROBE_TRACE`: no real exception/crash pattern was found in the safe trace.

## Asset Loading Findings

- `PROBE_TRACE`: the rejected file-hook run shows original file reads for:
  - `SAMP\samp.IDE`
  - `SAMP\custom.IDE`
  - `SAMP\CUSTOM.IMG`
  - `SAMP\SAMP.IMG`
  - `SAMP\SAMPCOL.IMG`
- `PROBE_TRACE`: original performs a large overlapped read against `SAMP\SAMPCOL.IMG`
  (`request=3301376`, `ERROR_IO_PENDING`). The earlier probe spam/crash came from our unsafe file-hook behavior, not from the original DLL alone.
- `STATIC_037 + PROBE_TRACE`: original imports D3DX mesh/texture helpers:
  `D3DXLoadMeshFromXA`, `D3DXCreateTextureFromFileA`,
  `D3DXCreateTextureFromFileExA`, `D3DXCreateTextureFromFileInMemory`,
  `D3DXCreateFontA`, `D3DXCreateSprite`.

## Probe Gaps

- `TODO_VERIFY`: current `samp_code_hook` RVAs are stale for this R5 DLL. Hooks for `samp.SocketLayer.SendTo`
  and `samp.ProcessNetworkPacket` did not install because expected prologues did not match.
- `TODO_VERIFY`: direct RPC-level trace needs either corrected RVAs or a verified callsite-level hook near
  `samp.dll+0x00053b19` / `samp.dll+0x00053a57`.
- `PROBE_TRACE + TODO_VERIFY`: asset trace should use path/open-only hooks first. The probe now separates
  `samp_probe_asset_paths.flag` / `SAMP_PROBE_ASSET_PATHS=1` from full `ReadFile` tracing via
  `samp_probe_file_hooks.flag` / `SAMP_PROBE_FILE_HOOKS=1`.
- `PROBE_TRACE + TODO_VERIFY`: full `ReadFile` hooks must stay opt-in and must preserve `LastError`, especially
  for overlapped IO.

## Next Recommended Trace

1. Run original DLL with `samp_probe_asset_paths.flag` enabled.
2. Capture complete `SAMP.IMG` / `CUSTOM.IMG` / `SAMPCOL.IMG` open order without ReadFile spam.
3. Add corrected, byte-validated hooks for internal networking functions only after function-entry RVAs are verified.
4. Diff the same scenario against the replacement DLL: entry gate, time patch, graphics loop target, camera mode,
   HUD flag, network packet sizes, and shutdown sequence.
