# ArchiveFS drop-in compatibility (2026-07-20)

## Scope

The replacement DLL now supplies `main.scm` and `script.img` from the signed
`samp.saa` archive. The physical files under `data/script/` are not renamed,
rewritten, or backed up.

## Evidence

- `STATIC_037`: original SA-MP 0.3.7 R5 `samp.dll`, SHA-256
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`.
- `STATIC_037`: archive construction at `samp.dll+0x65350` fixes 256 entries
  and 120 fake-header words. Entry loading at `samp.dll+0x64AD0` hashes the
  archive after the 256-byte header through the end of the encrypted entry
  table, verifies the 128-byte RSA/SHA-1 signature, and decrypts the table.
- `STATIC_037`: file wrappers are at `samp.dll+0x62670` (`ReadFile`),
  `samp.dll+0x627F0` (`GetFileSize`), `samp.dll+0x62840`
  (`SetFilePointer`), `samp.dll+0x628E0` (`CloseHandle`), and
  `samp.dll+0x62920` (`GetFileType`).
- `STATIC_037`: lower-cased basename hashes are Jenkins hashes initialized with
  `0x9E3779B9` and seeded with `0x12345678`. `main.scm` hashes to
  `0xD83F24DD`; `script.img` hashes to `0x11A462D1`.
- `STATIC_037`: fake handles start at `0xFF000001`, advance by two, and use a
  50-record table. EOF reads succeed with zero/partial byte counts and set
  `ERROR_HANDLE_EOF`; `FILE_END` seeks to the exact EOF and ignores the
  supplied distance.
- `PROBE_TRACE`: replacement run in
  `/home/chairman/Games/san-andreas-multiplayer-legacy` on 2026-07-20. GTA SA
  executable SHA-256
  `a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`.
  The runtime-tested replacement DLL SHA-256 is
  `75144869759dd0222e102c09eb7085fcb72e82fe7e3a7f67527c380a14d59599`.
- `PROBE_TRACE`: that run logged successful archive signature validation,
  all six GTA IAT hooks, virtual `main.scm` opens, the loaded multiplayer
  frontend, and `SA-MP 0.3.7-R5 Started`. An immediately preceding build of
  the same ArchiveFS implementation also logged virtual `script.img` opens and
  reached `RPC_ClientJoin`, `InitGame`, and `RakNet state CONNECTED`. The only
  subsequent behavior change made basename hashing retain trailing spaces, as
  the original does. The final built/installed DLL SHA-256 is
  `b1b9ff729aa430b92b2a9a23c9b69c8fc34ac35684a3ec3f656f38035ddf830f`;
  compared with the runtime-tested build, its remaining delta is a
  warning-clean procedure-pointer assignment with no ArchiveFS semantic
  change. It was rebuilt and statically checked but, per the agreed stopping
  point, not launched again.

No new original-DLL runtime run was performed for this change. Original-DLL
claims above are static R5 findings; the replacement runtime observations are
kept separate.

## Implementation

The portable archive core:

- validates the v2 header, archive bounds, entry-table location, block
  alignment, signature size, entry links, and all derived data offsets;
- refuses to expose entries until the Win32 adapter reports a successful
  signature verification;
- decrypts the entry table and file blocks with the original stateful cipher;
- normalizes only the basename, performs ASCII lower-casing, and calculates
  the original hash;
- eagerly decrypts the two required payloads so archive parsing and allocation
  failures happen before GTA starts loading scripts.

The Win32 adapter verifies the exact GTA SA 1.0 US PE profile and the current
contents of these IAT slots before writing them:

| API | GTA RVA |
| --- | ---: |
| `GetFileSize` | `gta_sa.exe+0x458074` |
| `CloseHandle` | `gta_sa.exe+0x458078` |
| `SetFilePointer` | `gta_sa.exe+0x458090` |
| `ReadFile` | `gta_sa.exe+0x458098` |
| `CreateFileA` | `gta_sa.exe+0x4580A0` |
| `GetFileType` | `gta_sa.exe+0x4581B4` |

Each slot must still equal the corresponding `kernel32.dll` procedure. Partial
installation is rolled back. Shutdown restores a slot only if it still points
to this DLL, so a later third-party hook is not overwritten. Ordinary file
names and non-virtual handles are passed to the original WinAPI procedures
without argument changes.

This scoped GTA-IAT method is intentionally narrower than the original DLL's
process-wide Kernel32 detours. It covers GTA's observed imports while avoiding
blindly patching third-party modules.

## Failure policy

- Multiplayer launch: a missing/rejected archive, failed payload extraction,
  unknown GTA executable, mismatched IAT target, or failed hook installation
  aborts DLL initialization with diagnostics. It does not continue into the
  incompatible physical single-player script.
- Offline launch: the same conditions leave the ordinary filesystem intact and
  log `filesystem_fallback`.
- `INFERRED` safety hardening rejects invalid negative/overflowing fake-handle
  seeks instead of reproducing the original unchecked pointer arithmetic.

## Checks

- Host `saa` test: pass. It validates the reference archive layout and hashes,
  refuses an unverified archive, extracts `main.scm` (103876 bytes) and
  `script.img` (194560 bytes), checks their file signatures, and rejects a
  corrupted header.
- MinGW/Win32 build through `reimpl/scripts/build_in_devbuild_toolbox.sh`: pass.
- Fresh-prefix script hashes before and after all runs were unchanged:
  - physical `main.scm`:
    `601def3baae766ce6a23e2f0b9b48f6b33c9a64e2fc32eb4f22ddea8b868b0fa`
  - physical `script.img`:
    `582b2ad47aafa9867238aff2d2b07cce5cc85496612157e759a4f25aa9e83187`
- Signed `samp.saa` fixture/runtime hash:
  `596895c61a70f558a9ad6b9b44cb5c9ac4b374e95ee2266ac9c88585898f8ae7`.

## Open points

- `TODO_VERIFY`: run a dedicated original-DLL probe with file-call tracing and
  normalize its fake-handle sequence against the replacement trace.
- `TODO_VERIFY`: exercise interior/shop transitions long enough to prove later
  `script.img` streaming behavior visually, not only its open/read path.
- `TODO_VERIFY`: expand virtualization to the remaining original
  `FILES_FORCE_FS` names when required; this change deliberately enables only
  `main.scm` and `script.img`.
