# Security Policy

## Scope

Libre-SAMP is compatibility research for a SA-MP 0.3.7-compatible `samp.dll`
replacement. It is intended for local development servers, controlled test
environments, preservation work, and protocol/runtime compatibility testing.

Do not use this project to cheat, evade bans, bypass server protections, attack
public servers, or disrupt other players.

## Supported Versions

Only the current `master` branch is actively handled during the experimental
phase. Tagged releases may receive security notes once release builds are
published.

## Reporting A Vulnerability

Prefer a private GitHub Security Advisory if the repository has advisories
enabled. If advisories are not available, open a minimal issue that says a
security report is available, but do not include exploit details publicly.

Useful report details:

- Commit hash or release artifact hash.
- Whether the issue affects `samp.dll`, `samp_probe.asi`, build scripts, or
  documentation.
- Reproduction steps using a local server where possible.
- Relevant logs with private paths, server addresses, tokens, and usernames
  removed.

## Server Data Handling

Server-provided data is untrusted. New packet/RPC handlers must:

- bounds-check all reads,
- validate string lengths and encodings,
- fail closed on malformed payloads,
- avoid exceptions across DLL or WinAPI boundaries,
- log unknown behavior with `TODO_VERIFY` instead of guessing unsafe semantics.

## Out Of Scope

The following are not accepted as security reports for this project:

- public-server ban evasion,
- anti-cheat bypass requests,
- cheat feature requests,
- attacks requiring proprietary binaries not shipped by this repository,
- reports that depend on violating GTA SA, SA-MP, or open.mp server terms.
