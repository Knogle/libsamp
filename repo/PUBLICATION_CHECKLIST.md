# Publication Checklist

This checklist describes the minimum clean state before the repository is made
public.

## P0 - Required Before Public

- [x] Project naming is consistent: `libsamp` as repository/artifact name,
      `Libre-SAMP` as the written-out name.
- [x] Goal is clear: drop-in replacement for `samp.dll`, not a full client
      installer and not a proprietary asset bundle.
- [x] No proprietary binaries or assets in the index. Project-generated loading
      screen assets are documented in `NOTICE.md`.
- [x] No local prefix paths, private usernames, or absolute home paths in
      documented example commands.
- [x] No non-public reference source trees in the repository.
- [x] No old reference path names or unused crosswalk files.
- [x] `.gitignore` covers local traces, builds, prefix copies, ZIPs, and working
      assets.
- [x] README clearly names reverse engineering of the original 0.3.7 DLL,
      golden traces, open.mp, gta-reversed, and ASI probe logs as sources.
- [x] Build instructions describe the reproducible devbuild path.
- [x] Runtime debug logs are limited by default and do not contain sensitive
      paths unless explicitly enabled for local analysis.
- [x] License status is clear: project-owned code, third-party components,
      generated data, and generated assets are documented separately.

## P1 - Strongly Recommended

- [ ] Bring `docs/re/` up to date: boot, netcode, spawn, TextDraw, dialog,
      vehicle, remote player, object pipeline.
- [ ] Fill `docs/traces/` with short normalized trace summaries.
- [ ] Standardize golden trace names:
      `load_init`, `connect_handshake`, `spawn`, `chat_dialog_textdraw`,
      `vehicle_state`, `remote_player_state`, `object_state`, `disconnect`.
- [ ] Add a compatibility matrix for tested servers and scenarios.
- [ ] Link known risks from the README instead of leaving them in chat history
      or external notes.
- [ ] Keep build artifacts clearly separate from source code.

## P2 - After Public

- [ ] CI for Linux host tools and Win32 cross build.
- [ ] Smoke test for RakNet/RPC decoder fixture bitstreams.
- [ ] Smoke test for TextDraw parser and color/alignment conversion.
- [ ] Enable lint/format checks for new C/C++ code only; avoid mass formatting.
- [ ] Optional trace sanitizer for logs before committing them.

## Public Messaging

Recommended short description:

> Libre-SAMP (`libsamp`) is an experimental drop-in replacement for the
> SA-MP 0.3.7 `samp.dll`, based on runtime traces, original DLL reverse
> engineering, open.mp protocol behaviour and GTA-SA engine references.

Do not promise:

- A full SA-MP client installer.
- Complete 1:1 parity.
- Compatibility with all mods/scripts.
- Anti-cheat bypass capabilities.
- Proprietary data in the repository.
