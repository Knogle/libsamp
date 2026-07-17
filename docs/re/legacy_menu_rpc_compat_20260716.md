# Legacy menu RPC compatibility (2026-07-16)

## Scope

This change implements the SA-MP 0.3.7 legacy menu path end to end:

- inbound RPC 76 `ScrInitMenu`
- inbound RPC 77 `ScrShowMenu`
- inbound RPC 78 `ScrHideMenu`
- outbound RPC 132 `MenuSelect`
- outbound RPC 140 `MenuQuit`

## Evidence

`STATIC_037`: original SA-MP 0.3.7-R5 `samp.dll`, SHA-256
`b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`:

- `ScrInitMenu`: `samp.dll+0x1C870`
- `ScrShowMenu`: `samp.dll+0x1CB90`
- `ScrHideMenu`: `samp.dll+0x1CC40`

The decoded layout uses a one-byte menu ID, 32-bit boolean fields, fixed
32-byte title/header/item buffers, 640x460 script coordinates, at most two
columns, twelve rows, and 128 pool slots. The replacement rejects invalid IDs,
counts, booleans, non-finite coordinates, and truncated buffers.

`OPENMP_REF`: the open.mp menu API documents the same 128-menu, two-column,
twelve-row, 32-byte text, and 640x460 coordinate limits. It also establishes
`OnPlayerSelectedMenuRow` and `OnPlayerExitedMenu` as the server-side results
of selection and exit.

## Test fixture

The bare gamemode command `/rpcmenu` creates and shows a two-column menu with
four rows. Row 2 is disabled to verify navigation filtering. Rows 0, 1, and 3
produce distinct server messages/actions after RPC 132. Escape produces a
server confirmation through RPC 140. `/rpcmenuhide` explicitly exercises RPC
78.

## Verification status

- Client Win32 build: passed.
- Bare Pawn compilation: passed without warnings.
- Live original/replacement trace comparison: `TODO_VERIFY`.
