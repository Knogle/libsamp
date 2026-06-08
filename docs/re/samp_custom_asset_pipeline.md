# SA-MP Custom Asset Pipeline

## Current Evidence

- `OBSERVED_037 + PROBE_TRACE`: original 0.3.7-R5 opens `SAMP\samp.IDE`, `SAMP\custom.IDE`, `SAMP\CUSTOM.IMG`, `SAMP\SAMP.IMG`, and `SAMP\SAMPCOL.IMG`.
- `PROBE_TRACE`: `SAMPCOL.IMG` uses large overlapped reads. Full `ReadFile` tracing must remain opt-in.
- `STATIC_037 + PROBE_TRACE`: original imports D3DX mesh/texture helpers including `D3DXLoadMeshFromXA` and texture creation helpers.
- `GTA_REVERSED_REF + INFERRED`: GTA object creation through script opcode `0107` assumes a valid `CModelInfo` entry. Our runtime currently skips SA-MP custom IDs because those entries are absent.
- `PROBE_TRACE + INFERRED`: replacement runs repeatedly encounter a broad set of SA-MP custom IDs from the server. The former canary whitelist has been removed so every indexed custom ID can reach the asset-readiness/registration gate.

## External Loader Findings

- `GTA_REVERSED_REF`: GTA SA 1.0 US exposes the native path we need:
  - `CStreaming::AddImageToList` at `0x407610` registers image file names in `ms_files`.
  - `CStreaming::LoadCdDirectory(const char*, StreamingImgID)` at `0x5B6170` reads `VER2` IMG directories, registers DFF entries by model name, creates TXD slots with `CTxdStore::FindTxdSlot` / `CTxdStore::AddTxdSlot`, and creates COL slots through `CColStore`.
  - `CModelInfo::GetModelInfo(const char*, int32*)` at `0x4C5940` resolves DFF names to existing model IDs.
  - `CModelInfo::AddAtomicModel` at `0x4C6620` creates a model info slot.
  - `CStreaming::RequestSpecialModel` at `0x409D10` loads a DFF from `ms_pExtraObjectsDir`, sets the texture dictionary to the model name when a matching TXD slot exists, then calls `RequestModel`.
- `GTA_REVERSED_REF`: `CStreaming::LoadCdDirectory` sends unknown DFF names into `ms_pExtraObjectsDir`. That is important for SA-MP models because the IDE stage must create `CModelInfo` entries before or around directory loading if we want regular model IDs (`>= 18631`) instead of only GTA special-model slots.
- `GTA_REVERSED_REF`: GTA's `CDirectory::DirectoryInfo` is the same 32-byte `VER2` directory shape we already index: sector offset, sector size and 24-byte filename.
- `MTA_REF` not used for this step. The GTA native path is sufficiently covered by gta-reversed and Mod Loader.
- `INFERRED`: D3D mesh helpers imported by the original SA-MP DLL are likely for UI/model preview or fallback paths, but custom world object rendering should first use GTA's streaming pipeline. Direct D3D rendering would bypass collision, draw distance, culling and entity lifetime.

## ASI / CLEO / Mod Loader Findings

- `GTA_REVERSED_REF + INFERRED`: CLEO-style object scripts ultimately rely on GTA model IDs being valid in `CModelInfo`; they are not enough for registering new archives.
- `GTA_REVERSED_REF`: SAMPFUNCS exposes a game API wrapper for `RequestSpecialModel(model, texture, channel)`, which confirms that SA-MP modding normally goes through GTA streaming rather than immediate-mode object drawing.
- `INFERRED`: plugin-sdk-based ASI examples are useful for call signatures and hooks, but the robust custom asset pattern is represented best by Mod Loader.
- `GTA_REVERSED_REF + INFERRED`: Mod Loader's standard streamer hooks GTA SA's `CStreaming::LoadCdDirectory` callsites (`0x5B6183`, `0x5B61B8`, `0x5B61E1`, `0x5B627A`, `0x5B6449`) and `CStreaming::RequestSpecialModel` callsites (`0x409F76`, `0x409FD9`) to inject custom archive directory entries into GTA's normal streaming bookkeeping.
- `INFERRED`: We should not copy Mod Loader's hook-heavy abstraction directly. For our fixed SA-MP archives, a smaller deterministic path is safer: explicitly add `SAMP.IMG`, `CUSTOM.IMG`, `SAMPCOL.IMG`, then feed their directories into GTA's own registration functions.

## Replacement State

- Vanilla GTA models can be created through the guarded object bridge.
- SA-MP custom models are gated before they reach `CStreaming::RequestModel`.
- `INFERRED + TODO_VERIFY`: current mutige test build can proxy indexed SA-MP custom object IDs
  through GTA model `3095` when their native `CModelInfo` entry is still absent. Logs keep both
  `model` and `render_model` so object lifetime/position can be validated without pretending that
  SAMP DFF/TXD registration is solved.
- `PROBE_TRACE + INFERRED + TODO_VERIFY`: the 2026-06-07 replacement run cleanly reached
  `active=128` proxied/script-created objects and still had pending server objects. The object
  bridge now defaults to a very aggressive cap of `512`, while `SAMPDLL_OBJECT_ACTIVE_CAP` and
  `SAMPDLL_OBJECT_CREATE_BUDGET` can lower/raise the test envelope without rebuilding.
- Runtime now logs a preflight line for each expected archive/IDE file:
  `samp_asset_preflight: label=... present=... size=... path=...`
- Runtime now builds a diagnostic-only IDE/IMG index:
  - `samp.IDE` / `custom.IDE`: `objs` and `tobj` map `model_id -> model_name, txd_name`.
  - `SAMP.IMG` / `CUSTOM.IMG` / `SAMPCOL.IMG`: `VER2` directory entries are indexed by filename.
  - `PROBE_TRACE + TODO_VERIFY`: observed `VER2` directory layout is `uint32 offset_sectors`, `uint32 size_sectors`, `char name[24]`.
  - `object_exception` lines for custom models include mapping and DFF/TXD/COL presence.
  - `samp_asset_custom_gate` logs a sample plus summary for all indexed SA-MP/custom models, including DFF/TXD readiness and whether `CModelInfo` already exists.
  - Skip reasons now distinguish `samp_custom_model_asset_incomplete` and `samp_custom_model_unregistered`.
  - This does not register `CModelInfo` entries and does not render custom SA-MP objects yet.

## Implementation Path

1. `PROBE_TRACE`: capture path-only original run with `samp_probe_asset_paths.flag`.
2. `STATIC_037`: identify original function-entry RVAs for IDE/IMG registration, not just file API callsites.
3. `GTA_REVERSED_REF`: implement a registration-only phase:
   - add `SAMP\SAMP.IMG`, `SAMP\CUSTOM.IMG`, and `SAMP\SAMPCOL.IMG` through `CStreaming::AddImageToList`;
   - parse `samp.IDE` / `custom.IDE` and create `CModelInfo::AddAtomicModel(model_id)` slots for object IDs with no existing `CModelInfo`;
   - set model name and texture dictionary metadata from IDE (`model_name`, `txd_name`);
   - run GTA-compatible directory registration for DFF/TXD/COL entries.
4. `TODO_VERIFY`: register archive entries without rendering first; validate `CModelInfo::ms_modelInfoPtrs[id] != 0` and `CTxdStore::FindTxdSlot(txd_name) != -1` for sampled custom IDs.
5. `TODO_VERIFY`: only then allow SA-MP custom object IDs through the existing object bridge. The bridge no longer has a canary whitelist, but it remains closed until `CModelInfo::ms_modelInfoPtrs[id] != 0`.

## Concrete Next Slice

1. `GTA_REVERSED_REF + TODO_VERIFY`: add a guarded `samp_asset_register_custom_models_compat()` function, disabled by default behind a runtime flag until one run validates init order.
2. `GTA_REVERSED_REF + TODO_VERIFY`: in that function, register only IDE model metadata first. Do not request or create objects yet.
3. `PROBE_TRACE`: log `samp_asset_registration: model=... model_info=... txd_slot=... img=... dff=... txd=... col=...`.
4. `TODO_VERIFY`: after a clean run, enable directory registration for `SAMP.IMG` / `CUSTOM.IMG`.
5. `TODO_VERIFY`: once DFF/TXD load cleanly, enable COL and then object creation.

## Guardrails

- Do not pass a custom model to `RequestModel` until model info is present.
- Keep `SAMPCOL.IMG` reads out of the ASI trace unless explicitly enabled.
- Prefer archive/IDE registration before any Direct3D mesh fallback; the original behaves like a GTA asset integration layer, not just an immediate-mode renderer.

## Source Links

- `GTA_REVERSED_REF`: https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Streaming.cpp
- `GTA_REVERSED_REF`: https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Models/ModelInfo.cpp
- `GTA_REVERSED_REF`: https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/TxdStore.cpp
- `INFERRED`: https://github.com/thelink2012/modloader/blob/master/src/plugins/gta3/std.stream/directory.cpp
- `INFERRED`: https://github.com/thelink2012/modloader/blob/master/src/plugins/gta3/std.stream/backend.cpp
- `INFERRED`: https://github.com/DK22Pac/plugin-sdk
