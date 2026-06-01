# SPEC-NET-002: Host/Port Parsing Rules

## Status

- Draft
- Spec owner: Team A
- Consumer: Team B

## Goal

Define deterministic parser behavior for endpoint input while preserving legacy IPv4 UX and adding IPv6 literal support.

## Input Forms

1. `hostname`
2. `hostname:port`
3. `ipv4-literal`
4. `ipv4-literal:port`
5. `ipv6-literal` (without explicit port)
6. `[ipv6-literal]:port`

## Rules

1. Empty input is invalid.
2. Bracket form must close (`[... ]`) and may optionally contain `:port`.
3. `hostname:port` is valid only when exactly one colon exists in non-bracket form.
4. Multiple colons in non-bracket form are treated as plain IPv6 literal without explicit port.
5. Empty host or empty port is invalid.
6. Port must be decimal and within `0..65535`.
7. If no explicit port is present, use caller-provided default port.

## Failure Behavior

1. Invalid syntax returns parse error and no partial output.
2. Out-of-range port returns parse error.

## Minimum Tests

1. `example.com:7777` -> host `example.com`, port `7777`
2. `203.0.113.10` with default `7777` -> host unchanged, port `7777`
3. `[2001:db8::10]:7777` -> host `2001:db8::10`, port `7777`
4. `2001:db8::10` with default `7777` -> host unchanged, port `7777`
5. `[2001:db8::10` -> error
6. `example.com:` -> error
7. `:7777` -> error
8. `example.com:99999` -> error
