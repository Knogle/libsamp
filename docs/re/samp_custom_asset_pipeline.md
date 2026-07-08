# SA-MP Custom Asset Pipeline

## Current Evidence

- `OBSERVED_037 + PROBE_TRACE`: original 0.3.7-R5 opens `SAMP\samp.IDE`, `SAMP\custom.IDE`, `SAMP\CUSTOM.IMG`, `SAMP\SAMP.IMG`, and `SAMP\SAMPCOL.IMG`.
- `OBSERVED_037 + PROBE_TRACE`: original 0.3.7-R5 run 2026-06-11 with GTA asset hooks registered images in this order:
  `SAMP\CUSTOM.IMG` -> image id `0`, `SAMP\SAMP.IMG` -> `1`, `MODELS\GTA3.IMG` -> `2`,
  `MODELS\GTA_INT.IMG` -> `3`, `SAMP\SAMPCOL.IMG` -> `4`, then script/cutscene/player archives.
  `LoadCdDirectory` used the same order, and model stores were already populated before the first directory load
  (`atomic=15417`, `time=160`, `clump=71`). The original therefore treats image id `0` as valid and prepares SA-MP
  model slots during early GTA init, not as a late per-object fallback.
- `OBSERVED_037 + PROBE_TRACE`: the same original run drove `CModelInfo::AddAtomicModel` 1417 times after
  `store_before >= 14000`, reaching `store_after=15417`, with no new exception in that run block. This is past
  gta-reversed's vanilla `NUM_ATOMIC_MODEL_INFOS = 14000`, but it is directly observed 0.3.7 behavior for SA-MP
  custom object assets.
- `OBSERVED_037 + PROBE_TRACE + GTA_REVERSED_REF`: the original `CPhysical::Add` hook saw custom object models such
  as `19894` from `samp.dll+0x0009fd52` with non-null `model_info_ptr`; the latest original run had no
  `model_info_ptr=00000000` samples in that hook. Custom objects must therefore not reach the physical-world path
  before their model info exists.
- `PROBE_TRACE`: `SAMPCOL.IMG` uses large overlapped reads. Full `ReadFile` tracing must remain opt-in.
- `STATIC_037 + PROBE_TRACE`: original imports D3DX mesh/texture helpers including `D3DXLoadMeshFromXA` and texture creation helpers.
- `GTA_REVERSED_REF + INFERRED`: GTA object creation through script opcode `0107` assumes a valid `CModelInfo` entry. Our runtime currently skips SA-MP custom IDs because those entries are absent.
- `PROBE_TRACE + INFERRED`: replacement runs repeatedly encounter a broad set of SA-MP custom IDs from the server. The former canary whitelist has been removed so every indexed custom ID can reach the asset-readiness/registration gate.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: the 2026-06-10 22:15 replacement run received the Area51 `ScrCreateObject` RPCs correctly, then crashed after `samp_asset_registration: proceed model=18631`. That points at the native custom-asset registration path, not the network decode.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: the 2026-06-10 22:51 replacement run progressed through connect,
  class/spawn, `mp_session_bridge: spawn_finalize`, and script processing, then crashed immediately after targeted
  `samp_asset_registration: proceed model=19313`. The fault was a write access violation during the experimental
  ModelInfo metadata fill, so those layout writes are now guarded by writable-memory checks and can be disabled with
  `SAMPDLL_CUSTOM_ASSET_METADATA=0`.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: the 2026-06-10 23:04 replacement run registered models `19313`,
  `19909`, and `19907`, then faulted at `0x004C675A` inside `CModelInfo::AddClumpModel` while processing the next
  pending custom model. The returned slots were clump-store indices 89, 90, and 91; gta-reversed documents
  `NUM_CLUMP_MODEL_INFOS = 92`. That crash is now explained as a bad local registration choice: SA-MP `objs`
  entries should follow GTA's regular object path, not the clump-object path.
- `GTA_REVERSED_REF + PROBE_TRACE + TODO_VERIFY`: gta-reversed `CFileLoader::LoadObject` parses normal IDE `objs`
  entries and creates `CModelInfo::AddAtomicModel` entries at `0x4C6620`; `CModelInfo::AddClumpModel` at
  `0x4C6740` belongs to the separate clump object loader. The replacement now chooses the ModelInfo add function
  from the parsed IDE section, so current SA-MP `objs` samples such as `19312`, `19313`, `19905`, `19907`, and
  `19909` no longer consume the 92-entry vanilla clump store.
- `GTA_REVERSED_REF + PROBE_TRACE + TODO_VERIFY`: `CBaseModelInfo` is a C++ object, so runtime offset 0 is its vtable.
  The metadata fill now writes `m_nKey` at offset 4 and `m_nTxdIndex` at offset 10; writing the key at offset 0
  corrupts the vtable.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: the 2026-06-11 10:44 replacement run reached object creation for
  Area51 model `19905`, then faulted at `gta_sa.exe+0x19F8B4` (`CObject::Init`) while reading
  `CBaseModelInfo::m_pColModel + 0x29`. gta-reversed places `m_pColModel` at offset 20, so custom objects must not
  enter GTA's `CreateObject` path until their collision model is bound.
- `OBSERVED_037 + PROBE_TRACE`: local `SAMP\SAMPCOL.IMG` is a `VER2` archive with one entry,
  `AllSAMPCOLs.col`. Per-model `modelname.col` lookup is therefore a bad diagnostic assumption for SA-MP objects;
  collision loading needs to feed that aggregate COL buffer through GTA's `CColStore`/`CFileLoader` path.

## External Loader Findings

- `GTA_REVERSED_REF`: GTA SA 1.0 US exposes the native path we need:
  - `CStreaming::AddImageToList` at `0x407610` registers image file names in `ms_files`.
  - `CStreaming::LoadCdDirectory(const char*, StreamingImgID)` at `0x5B6170` reads `VER2` IMG directories, registers DFF entries by model name, creates TXD slots with `CTxdStore::FindTxdSlot` / `CTxdStore::AddTxdSlot`, and creates COL slots through `CColStore`.
  - `CModelInfo::GetModelInfo(const char*, int32*)` at `0x4C5940` resolves DFF names to existing model IDs.
  - `CModelInfo::AddAtomicModel` at `0x4C6620` is the matching path for regular IDE `objs` entries.
  - `CModelInfo::AddTimeModel` at `0x4C66B0` is the matching path for IDE `tobj` entries.
  - `CModelInfo::AddClumpModel` at `0x4C6740` is the matching path for explicit clump object entries.
  - `CStreaming::RequestSpecialModel` at `0x409D10` loads a DFF from `ms_pExtraObjectsDir`, sets the texture dictionary to the model name when a matching TXD slot exists, then calls `RequestModel`.
  - `CColStore::AddColSlot` at `0x411140` and `CColStore::LoadCol(int32, uint8*, int32)` at `0x4106D0` can load an
    in-memory COL buffer. gta-reversed shows the first-load path uses `CFileLoader::LoadCollisionFileFirstTime` and
    binds `CColModel` instances to `CModelInfo` by model ID/name.
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
- Runtime builds a lazy diagnostic IDE/IMG index on first custom-object need, not in early bootstrap:
  - `samp.IDE` / `custom.IDE`: `objs` and `tobj` map `model_id -> model_name, txd_name`.
  - `SAMP.IMG` / `CUSTOM.IMG` / `SAMPCOL.IMG`: `VER2` directory entries are indexed by filename.
  - `PROBE_TRACE + TODO_VERIFY`: observed `VER2` directory layout is `uint32 offset_sectors`, `uint32 size_sectors`, `char name[24]`.
  - `object_exception` lines for custom models include mapping and DFF/TXD/COL presence.
  - `samp_asset_custom_gate` logs a sample plus summary for all indexed SA-MP/custom models, including DFF/TXD readiness and whether `CModelInfo` already exists.
  - Skip reasons now distinguish `samp_custom_model_asset_incomplete` and `samp_custom_model_unregistered`.
- `GTA_REVERSED_REF + PROBE_TRACE + TODO_VERIFY`: the 2026-06-10 custom-object slice guards a
  native SA-MP asset registration path behind `SAMPDLL_CUSTOM_ASSET_REGISTER`:
  - parse the local IDE/IMG index;
  - create indexed `CModelInfo` slots only after the multiplayer scene is loaded, using `AddAtomicModel` for
    parsed `objs` rows and `AddTimeModel` for parsed `tobj` rows;
  - keep the server-requested object queue as a retry/backfill path for IDs that arrive before the bulk
    registration point;
  - fill the inferred `CBaseModelInfo` key / TXD index / draw-distance fields unless disabled with
    `SAMPDLL_CUSTOM_ASSET_METADATA=0`;
  - register `SAMP\SAMP.IMG`, `SAMP\CUSTOM.IMG`, and `SAMP\SAMPCOL.IMG` with
    `CStreaming::AddImageToList`;
  - load their directories once through `CStreaming::LoadCdDirectory` after model infos exist.
- `OBSERVED_037 + PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: after the clump-store crash was traced to using
  `AddClumpModel` for regular `objs` rows, broad indexed-model registration exposed our too-strict vanilla
  atomic-store guard. The original 2026-06-11 run shows SA-MP 0.3.7 lets GTA's early IDE loader drive the atomic
  store counter to `15417`. A replacement 2026-07-08 run successfully registered and collision-bound several
  targeted SA-MP custom batches, then skipped ready models such as `18997`, `19425`, `18842`, and `18818` solely
  because the diagnostic guard still capped the atomic store at vanilla `14000`. A later 2026-07-08 libsamp
  replacement run proved that simply lifting this cap is not safe yet: `CModelInfo::AddAtomicModel` faulted at
  `0x004c663b` as soon as the unexpanded GTA atomic store reached `14000`. The replacement therefore keeps
  `SAMPDLL_CUSTOM_ASSET_OVER_VANILLA` off by default until the original SA-MP store expansion/pointer patch is
  reproduced. The real compatibility path is still the original-like early asset registration phase.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: targeted registration now checks `ms_clumpModelInfoStore` capacity
  only before true `AddClumpModel` calls. Regular SA-MP `objs` registration uses `AddAtomicModel`, which matches
  GTA's IDE loader and avoids the 92-entry clump-store ceiling seen in the 2026-06-11 11:04 run. The bulk path also
  guards the atomic/time/clump store counters before calling GTA's `Add*Model` functions.
- `OBSERVED_037 + PROBE_TRACE + TODO_VERIFY`: the 2026-07-08 original-R5 run shows broad custom AtomicModelInfo
  creation first, then `CUSTOM.IMG`/`SAMP.IMG`/`SAMPCOL.IMG` directory loads, then COL binding. The replacement keeps
  `SAMPDLL_CUSTOM_ASSET_BULK` enabled by default now and drains pending registration immediately after spawn scene-load;
  set `SAMPDLL_CUSTOM_ASSET_BULK=0` to return to targeted-only triage.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: targeted registration now attempts a one-shot direct
  `AllSAMPCOLs.col` load from `SAMP\SAMPCOL.IMG` after requested custom `CModelInfo` entries exist, and the object
  bridge defers any `CreateObject` call whose render model still has no readable `m_pColModel`.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: the 2026-06-11 13:53 replacement run showed the one-shot COL load
  happened after the first five targeted custom `CModelInfo` entries, but later targeted registrations still had
  `col_model=0x00000000`. The replacement now keeps the same `AllSAMPCOLs` slot and retries the GTA
  collision load after later registration batches, logging `registered=a->b`, `col_bound=a->b`, and the selected
  loader. After the 2026-06-11 attach run left `foamhoop1`, `tube50mglass180bend`, `funboxramp1`, and
  `tube50mglass90bend1` unbound, retries now call `CFileLoader::LoadCollisionFileFirstTime` directly instead of
  re-entering `CColStore::LoadCol` on an already-loaded slot; gta-reversed shows the later slot-loaded path searches
  only inside the slot's recorded model range, while the first-time path performs the global model-name lookup.
- `GTA_REVERSED_REF + PROBE_TRACE + TODO_VERIFY`: targeted custom model registration now records `objs`
  draw-distance/flags from `SAMP.ide` and can reload the SA-MP IMG directories after later targeted `CModelInfo`
  batches. This keeps late-registered models from depending on a single directory parse that may have happened
  before their `CModelInfo` slot existed. Reloads are capped by `SAMPDLL_CUSTOM_ASSET_DIR_RELOADS` and default to
  four attempts; this is still a compatibility experiment, not a proven original-R5 timing match.
- `OPENMP_REF + GTA_REVERSED_REF + TODO_VERIFY`: `ScrCreateObject` RPC 44 attachment metadata is now decoded from
  the open.mp object payload shape and kept on the object slot. Vehicle/object parents are applied after GTA object
  creation through GTA script attachment opcodes and retried after object/vehicle flushes. RPC 75
  `AttachObjectToPlayer` is decoded and stored, but actual actor attachment remains deferred until the SA-MP
  player-actor mapping and original 0.3.7 behavior are verified.
- `PROBE_TRACE + TODO_VERIFY`: Area51 object runs should now log `samp_asset_registration: request`,
  `samp_asset_registration: drain`, `archive_add`, and `archive_dir_loaded`, then allow custom IDs
  such as `19312`, `19313`, `19905`, `19907`, and `19909` past the object bridge if
  `CModelInfo::ms_modelInfoPtrs[id] != 0`.
- `PROBE_TRACE + OBSERVED_037 + TODO_VERIFY`: the 2026-06-11 11:24 Area51 run still skipped model
  `11692` with `missing_model_info`. Local `SAMP\SAMP.ide` maps it to
  `A51LandBit1, a51_ext`, and `SAMP\SAMP.IMG` contains `A51LandBit1.dff`, so the custom-object gate
  must treat parsed SA-MP/custom IDE rows as multiplayer asset models even below the former
  high-ID cutoff `18631`.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: the 2026-06-10 21:18 run decoded `ScrCreateObject`
  correctly, but all sampled Area51 models still skipped with `samp_custom_model_unregistered`.
  The same rows showed their DFF files present in `SAMP.IMG`, so the registration path now creates
  targeted `CModelInfo` entries from IDE metadata after the local index confirms the
  required DFF/TXD inputs; `CStreaming::LoadCdDirectory` remains responsible for binding DFF/TXD data.

## Implementation Path

1. `PROBE_TRACE`: capture path-only original run with `samp_probe_asset_paths.flag`.
2. `STATIC_037`: identify original function-entry RVAs for IDE/IMG registration, not just file API callsites.
3. `GTA_REVERSED_REF`: implement a registration-only phase:
   - add `SAMP\SAMP.IMG`, `SAMP\CUSTOM.IMG`, and `SAMP\SAMPCOL.IMG` through `CStreaming::AddImageToList`;
   - parse `samp.IDE` / `custom.IDE` and create section-appropriate `CModelInfo` slots for object IDs with no existing `CModelInfo`;
   - set model name and texture dictionary metadata from IDE (`model_name`, `txd_name`);
   - run GTA-compatible directory registration for DFF/TXD/COL entries.
4. `TODO_VERIFY`: register archive entries without rendering first; validate `CModelInfo::ms_modelInfoPtrs[id] != 0` and `CTxdStore::FindTxdSlot(txd_name) != -1` for sampled custom IDs.
5. `TODO_VERIFY`: only then allow SA-MP custom object IDs through the existing object bridge. The bridge no longer has a canary whitelist, but it remains closed until `CModelInfo::ms_modelInfoPtrs[id] != 0`.

## Concrete Next Slice

1. `STATIC_037 + PROBE_TRACE + TODO_VERIFY`: keep mapping the original early-init IDE/archive function RVAs so
   the replacement can move custom model registration from late object fallback toward the original startup path.
2. `PROBE_TRACE + TODO_VERIFY`: run custom and original prefixes on the same Area51 script and diff
   object visibility plus `samp_asset_registration` / `object_exception` lines.
3. `TODO_VERIFY`: verify `CModelInfo::ms_modelInfoPtrs[id] != 0`, TXD slot, and DFF streaming state
   for sampled custom IDs after directory load.
4. `TODO_VERIFY`: implement COL registration/validation if world objects render but have missing or
   wrong collision.
5. `OPENMP_REF + PROBE_TRACE + TODO_VERIFY`: verify attached custom objects on original and replacement runs,
   especially vehicle/object parent ordering and player attachments sent through RPC 75.
6. `OPENMP_REF + PROBE_TRACE + TODO_VERIFY`: implement `RemoveBuildingForPlayer` beyond decode-only,
   since the Area51 script sends RPC 43 before the custom objects.

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
