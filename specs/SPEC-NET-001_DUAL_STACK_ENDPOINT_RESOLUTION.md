# SPEC-NET-001: Dual-Stack Endpoint Resolution and Connect

## Status

- Draft
- Spec owner: Team A
- Consumer: Team B

## Scope

Define replacement behavior for hostname/IP resolution and outbound connect path currently implemented with IPv4-only Winsock 1 APIs.

## Current Behavior (Observed)

1. Winsock runtime is initialized once with `WSAStartup(2.2)` and torn down via `WSACleanup`.
2. Host resolution relies on `gethostbyname`.
3. Address parsing relies on `inet_addr`/`inet_ntoa`.
4. Socket family is `AF_INET`.
5. Connect path constructs `sockaddr_in` and calls `connect`.
6. Error path maps failed resolution/connect to user-facing connect failure strings.

## Required Behavior (Reimplementation Target)

1. Support IPv4 and IPv6 endpoints for outbound connections.
2. Preserve compatibility with existing IPv4 servers and existing host:port UX.
3. Resolve names via `getaddrinfo` with `AF_UNSPEC`.
4. Iterate resolved addresses in deterministic order.
5. Use OS result order first.
6. If one attempt fails, continue with the next candidate.
7. Preserve existing timeout semantics (or stricter, but not longer).
8. Keep current user-visible states/messages for connect success/failure.
9. Maintain non-blocking or asynchronous behavior expectations already present in the client.

## API-Level Requirements

1. Replace `gethostbyname` with `getaddrinfo`.
2. Use `sockaddr_storage` for internal endpoint representation.
3. Support IPv6 literal syntax with brackets when paired with port: `[2001:db8::1]:7777`.
4. Support plain DNS names and IPv4 literals exactly as before.
5. If both A and AAAA are present, attempt all records until success or exhaustion.
6. Free resolver results with `freeaddrinfo` on all paths.

## Failure Handling Requirements

1. Resolution failure returns an error equivalent to "failed to connect host".
2. Socket creation failure on one candidate does not stop trying remaining candidates.
3. Connect failure on one candidate does not stop trying remaining candidates.
4. If all candidates fail, return failure and preserve legacy reconnect/error flow.

## Test Matrix (Minimum)

1. IPv4 literal success: `203.0.113.10:7777`.
2. IPv6 literal success: `[2001:db8::10]:7777`.
3. DNS A-only success.
4. DNS AAAA-only success.
5. DNS dual-stack where first record fails and second succeeds.
6. Invalid hostname.
7. Closed port timeout.
8. Bracket parsing failures (`[2001:db8::1`, `2001:db8::1:7777` without bracket rule).

## Evidence Pointers

- `analysis/metadata/NETWORK_SURVEY.md`
- `analysis/notes/fcn_10053870_connect_ipv4.txt`
- `analysis/notes/fcn_100538b0_socket_setup_bind.txt`
- `analysis/notes/fcn_100539c0_resolve_helper.txt`
- `analysis/notes/fcn_10055ff0_connect_worker.txt`
