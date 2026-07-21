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
- `OBSERVED_037 + PROBE_TRACE`: the 2026-07-09 heavy original-prefix probe showed the low SA-MP custom range
  `11682..11753` backed by valid `ms_modelInfoPtrs` before archive directory loading. The slots used
  `0x00A9B0C8 + model * 4` (for example model `11683` at `0x00AA6754`), and the returned AtomicModelInfo pointers
  were contiguous with `0x20` stride. `LoadCdDirectory(SAMP\CUSTOM.IMG)` did not increase the model store counters,
  so this registration happens before archive directory loading. Model `11753` was the last original atomic-store
  allocation, moving the count from `15416` to `15417`.
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
- `PROBE_TRACE + TODO_VERIFY`: offline inspection of the local `SAMP` sidecars on 2026-07-10 found
  `SAMP.img` with 1673 entries (`1462` DFF, `208` TXD, `3` IFP), `SAMPCOL.img` with one aggregate
  `AllSAMPCOLs.col`, and `SAMP.ide` with `1433` `objs`/Atomic rows: `72` in `11682..11753` plus `1361`
  sparse rows in `18631..19999`; `CUSTOM.ide` is empty. Two additional `anim` rows, `19901` and `19902`,
  are clump models and are not part of the 1433-entry Atomic pass. The replacement parser still ignores `anim`, so
  those two models remain a separate open point. The low range has no missing DFF or COL names, but `46/72` rows
  reference TXDs that are not in `SAMP.img` because they are vanilla GTA TXDs such as `csmafcouch`, `int_casinoint3`,
  and `a51_ext`. Replacement diagnostics must therefore treat `txd_file=missing` separately from an actually
  unavailable `CTxdStore` slot.
- `PROBE_TRACE + TODO_VERIFY`: the 2026-07-09 libsamp scan run registered the first nine pending Area51/tube
  models and bound their COLs, then successfully registered/created/destroyed scan model `11682`. The crash occurred
  on model `11683` during the second late `LoadCdDirectory(SAMP\SAMP.IMG)` reload, not while reading the asset files.
  This supports pre-registering the observed low range before one directory load instead of reloading `SAMP.img` once
  per scanned low-range object.
- `PROBE_TRACE + TODO_VERIFY`: the 2026-07-10 libsamp run with early low-range synthetic registration crashed during
  spawn-finalize after registering `19905` and `18809`, immediately before the pending low model `11692` would enter
  the synthetic path (`ip=0xffff0000`). At that stage low-range bulk fill stayed opt-in and requested low IDs used
  native `AddAtomicModel`; the later relocated-store implementation supersedes that historical restriction.
- `PROBE_TRACE + TODO_VERIFY`: the later 2026-07-10 libsamp run with native low-ID registration reached class
  selection, spawn-finalize, registered and COL-bound the nine pending Area51/tube models, created all 19 startup
  objects, then `/sampobjscan 11682 5000 3000` successfully created and destroyed the first scan object. It crashed on
  the second scan object, model `11683`, during the second late `LoadCdDirectory(SAMP\SAMP.IMG)` replay. Late directory
  replays therefore default to one pass; later registrations still retry direct `AllSAMPCOLs.col` binding.
- `PROBE_TRACE + GTA_REVERSED_REF + INFERRED + TODO_VERIFY`: the 2026-07-10 13:06 replacement build applied the
  relocated atomic store with `capacity=15417`, `constructed=15417`, and `patched=35/35`. The scan then registered
  and created models `11682..11688`; all seven ModelInfos existed before the single targeted `SAMP.IMG` replay.
  Model `11689` was registered afterward at atomic-store index/count `14000`, followed by successful native additions
  through count `14006`, but its directory replay logged `archive_dir_reload_skip ... reloads=1 max=1`. The final
  marker was `object: create_begin` for `11689`, before `RequestModel`, `LoadAllRequestedModels`, and opcode `0107`.
  This exonerates `AddAtomicModel` at the vanilla boundary and points instead to a missing DFF `CStreamingInfo`
  directory mapping for models registered after the last replay.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: the 2026-07-10 14:14:46 replacement build pre-registered the complete
  low range `11682..11753` before one targeted `SAMP.IMG` replay. All 72 scan models reached loaded DFF state and
  completed opcode `0107`; the former stop at `11689` succeeded with a non-null RW object. The next indexed Atomic
  model is `18631` because `11754..18630` has no stock `objs` rows. Models `18631..18635`, registered only after the
  last directory pass, retained DFF `img=0, cd_size=0` and were safely deferred by `streaming_dff_unmapped` instead
  of entering GTA creation. The run ended cleanly through `/q` and `process_detach`.

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
- `INFERRED + TODO_VERIFY`: CLEO+ `LOAD_SPECIAL_MODEL` is useful as a RenderWare/TXD reference, but it creates an
  in-memory special model from a named DFF/TXD pair rather than a normal GTA `CModelInfo`/streaming/COL entry. That may
  be a future diagnostic renderer, but it is not parity for SA-MP world objects driven through `CreateObject`.
- `PROBE_TRACE + INFERRED + TODO_VERIFY`: the replacement now has an explicit opt-in parallel-renderer staging mode,
  `SAMPDLL_CUSTOM_OBJECT_VISUAL_FALLBACK=1`. It keeps blocked custom objects as visual-only SA-MP object slots and draws
  projected markers through the existing D3D EndScene overlay. This is not DFF mesh rendering yet; it is the lifecycle
  and render-hook foundation for a CLEO+-style DFF/TXD renderer.

## Replacement State

- Vanilla GTA models can be created through the guarded object bridge.
- SA-MP custom models are gated before they reach `CStreaming::RequestModel`.
- `OBSERVED_037 + PROBE_TRACE`: stock rows from the shipped `SAMP/samp.IDE`
  are registered on demand through the validated heap path even when the
  experimental `SAMPDLL_CUSTOM_ASSET_REGISTER` switch is off. Rows supplied by
  `SAMP/custom.IDE` remain opt-in. This separation restores the 19/19 bare-server
  startup-object path without enabling arbitrary downloaded model registration.
- `PROBE_TRACE + GTA_REVERSED_REF`: a stock `samp.IDE` TXD reference is ready
  when it resolves in GTA's `CTxdStore`, even if no matching TXD file exists in
  `SAMP.IMG`. The Area51 models use the vanilla `a51_ext` dictionary, observed
  as TXD slot `2007` before object-scene readiness.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: native Windows GTA SA 1.0 has
  eight `CStreaming::ms_files` entries and already used six when the current
  late registration pass ran. `CUSTOM.IMG` and `SAMP.IMG` occupy the final two;
  `SAMPCOL.IMG` is therefore kept out of the streaming list and loaded through
  the existing direct `AllSAMPCOLs.col` path. `AddImageToList` result zero is
  validated against the descriptor path because it also means "list full".
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: the same native Windows run
  blocked inside synchronous `CStreaming::LoadAllRequestedModels` for model
  `11692` after a successful late SAMP DFF request, while the identical archive
  completed under Wine. Custom DFF requests now remain on GTA's asynchronous
  request list and object creation waits for a later tick to observe `LOADED`
  plus a readable RW object. Vanilla GTA object models retain synchronous load.
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
  - register `SAMP\CUSTOM.IMG`, `SAMP\SAMP.IMG`, and `SAMP\SAMPCOL.IMG` with
    `CStreaming::AddImageToList`;
  - load their directories once through `CStreaming::LoadCdDirectory` after model infos exist.
- `OBSERVED_037 + PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: after the clump-store crash was traced to using
  `AddClumpModel` for regular `objs` rows, broad indexed-model registration exposed our too-strict vanilla
  atomic-store guard. The original 2026-06-11 run shows SA-MP 0.3.7 lets GTA's early IDE loader drive the atomic
  store counter to `15417`. A replacement 2026-07-08 run successfully registered and collision-bound several
  targeted SA-MP custom batches, then skipped ready models such as `18997`, `19425`, `18842`, and `18818` solely
  because the diagnostic guard still capped the atomic store at vanilla `14000`. A later 2026-07-08 libsamp
  replacement run proved that simply lifting this cap is not safe yet: `CModelInfo::AddAtomicModel` faulted at
  `0x004c663b` as soon as the unexpanded GTA atomic store reached `14000`. The replacement now applies an
  Open-Limit-Adjuster-style fixed `CAtomicModelInfo` store relocation during the pregame patch phase: allocate a
  `0x20`-stride store, construct every slot with GTA's `CAtomicModelInfo` constructor at `0x4C5540`, patch the GTA-SA
  AtomicModels pointer sites, and read the store count from the relocated store. This follows OLA's `ForceCapacity`
  behavior more closely than copying early `0xAAE950` bytes, which crashed at `0x004C686D` when the original static
  store had not been constructed yet. Default target capacity is the observed SA-MP ceiling `15417`
  (`SAMPDLL_ATOMIC_MODEL_STORE_CAPACITY` can override for probes). The 2026-07-10 13:06 run validated construction,
  all 35 active OLA pointer sites, and native additions through count `14006`; the 14:14 run extended that validation
  through count `14064` and created all 72 low-range models. Filling the complete stock catalog to `15417` remains
  `TODO_VERIFY`.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: targeted registration now checks `ms_clumpModelInfoStore` capacity
  only before true `AddClumpModel` calls. Regular SA-MP `objs` registration uses `AddAtomicModel`, which matches
  GTA's IDE loader and avoids the 92-entry clump-store ceiling seen in the 2026-06-11 11:04 run. The bulk path also
  guards the atomic/time/clump store counters before calling GTA's `Add*Model` functions.
- `OBSERVED_037 + PROBE_TRACE + INFERRED + TODO_VERIFY`: the 2026-07-08 original-R5 run shows broad custom AtomicModelInfo
  creation first, then `CUSTOM.IMG`/`SAMP.IMG`/`SAMPCOL.IMG` directory loads, then COL binding. The replacement keeps
  `SAMPDLL_CUSTOM_ASSET_BULK` enabled and now defaults `SAMPDLL_CUSTOM_ASSET_BULK_LIMIT` to the exact 1433 stock
  `objs` rows. With the relocated store active, those sparse indexed rows are registered in IDE parse order before
  pending server IDs and before the first archive directory pass. This matches the observed `13984 + 1433 = 15417`
  Atomic count; the environment limit and targeted low-range helper remain diagnostic fallbacks.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: replacement build `Jul 10 2026 14:59:51`,
  SHA256 `9207a814c64d2af7dd4d96a3a8081c1865b4d59b017e92f6b273e025ceaaaa3b`, registered all 1433 stock Atomic rows
  with `skipped=0`, moved the store from `13984` to `15417`, loaded the three archive directories once, and bound
  collision for `1433/1433` models. The completed `18632..19999` scan produced loaded DFFs, non-null RW objects,
  and successful opcode `0107` creates without streaming failures or exceptions. Models `19279..19281`, initially
  cancelled by fast destroy RPCs before client processing, each passed the same load/create checks in a slower
  follow-up run. Together with the prior `11682..11753` pass, this validates every renderable stock Atomic row;
  `18631` is the intentional `NoModelFile` placeholder and `19901/19902` remain separate `anim`/clump work.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: the 2026-07-09 libsamp run showed ready elevator custom models
  `18755`, `18756`, and `18757` with IDE, DFF, TXD, and `AllSAMPCOLs.col` present, but later object creation skipped
  them as `samp_custom_model_registration_blocked` after the earlier broad pass reached the vanilla atomic cap
  (`model_info_store_full`, count `14000`). The former pending-first/default-zero policy protected that unexpanded
  store; the relocated 15417-entry store and exact 1433-row stock pass supersede it in the normal path.
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
  before their `CModelInfo` slot existed. Reloads are capped by `SAMPDLL_CUSTOM_ASSET_DIR_RELOADS` and default to one
  late replay after the initial archive load, because repeated `SAMP.IMG` replays crashed in the current scan probe.
- `OBSERVED_037 + PROBE_TRACE + TODO_VERIFY`: targeted low-range registration now defaults to one native pass over
  the complete `11682..11753` range before the single late directory replay, independent of which low model is
  requested first. `SAMPDLL_CUSTOM_LOW_PRELOAD` defaults to `72`; smaller values retain the forward-window probe
  behavior. The helper stops at the relocated AtomicModelInfo capacity and remains disabled during `spawn_finalize`;
  with full stock bulk registration enabled it should no longer be needed in the normal path.
- `PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY`: immediately before custom-object streaming the bridge now snapshots
  ModelInfo vtable/key/TXD/COL/draw-distance/RW-Atomic plus DFF and TXD `CStreamingInfo` flags, IMG id, CD offset/size,
  and load state. Separate begin/end markers bracket `RequestModel`, `LoadAllRequestedModels`, and opcode `0107`.
  An indexed custom DFF with no CD mapping is left pending before `RequestModel`; after the synchronous load, custom
  creation is deferred with a 250 ms backoff unless load state is `LOADED` and `m_pRwObject` is readable.
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

## Object material pipeline

Original static reference: SA-MP 0.3.7-R5 `samp.dll`, SHA256
`b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`.
The claims below tagged `STATIC_037` come from the existing Ghidra analysis plus direct disassembly of exactly
that binary; no material-specific original runtime hook has corroborated them yet.

- `STATIC_037`: RPC 44 at `samp.dll+0x1B340` dispatches its trailing material list through
  `samp.dll+0x1B070`; RPC 84 enters at `samp.dll+0x1B6A0`. Both populate the same 16 persistent
  per-object slots. Type-1 records contain source model, TXD, texture and wire-ABGR colour. Type-2 records
  contain font/text parameters and up to `0x800` bytes of RakNet-compressed text.
- `STATIC_037`: `CObject::SetMaterial` at `samp.dll+0xA8530` resolves the source texture. Positive source
  models are requested with flag `2`, polled up to 250 times with `Sleep(2)`, and released afterward only when
  they had no prior model reference and are resident. Model `-1` is colour-only. Model `0` uses the separately
  loaded `SAMP\\blanktex.txd` path through
  `samp.dll+0xB2B90`, with the cached texture pointer at `samp.dll+0x1A2600`.
- `STATIC_037`: callbacks at `samp.dll+0xA8AD0`, `+0xA8840`, and `+0xA8B50` capture, apply, and restore
  materials around every object draw (`+0xA8C00` / `+0xA9E70`). The material index restarts for every Atomic.
  The original therefore never leaves a replacement texture or colour on GTA's shared `RpGeometry`.
- `STATIC_037 + GTA_REVERSED_REF + TODO_VERIFY`: GTA `CAtomicModelInfo::CreateInstance` clones the Atomic but shares its
  Geometry. The replacement consequently patches only the validated `CObject::Render` vtable entry
  `0x866FA8` (index 18, original pointer `0x59F180`) and restores that pointer on shutdown. Immediately before
  the original render it swaps resolved slot textures/colours and the `rpGEOMETRYMODULATEMATERIALCOLOR` flag;
  immediately afterward it restores texture, colour, surface properties, and geometry flags. Runtime validation
  must still prove the hook target and restoration path in Wine.
- `PROBE_TRACE + GTA_REVERSED_REF + INFERRED + TODO_VERIFY`: the 2026-07-10 SuperFreeroam `/ls` teleport run
  completed 17 first material draws and then faulted at GTA `0x757D6E`, where the D3D9 All-In-One instancing path
  dereferenced a null stream-0 vertex buffer immediately before `IDirect3DVertexBuffer9::Lock`. Unlike R5's own
  pre/post-draw lifecycle, the replacement vtable hook can enter before a Geometry has its first `RwResEntry`.
  The replacement therefore applies a resolved texture immediately but defers material-colour/surface-property and
  Geometry-flag mutation until `geometry->repEntry` contains a readable D3D9 header with a non-null stream-0 vertex
  buffer. `object_material: render_begin` records Geometry flags, resource entry, header, serial, mesh count, both
  vertex streams, and readiness before calling GTA; `render_apply` records the post-render stream state.
- `PROBE_TRACE`: the 2026-07-10 SuperFreeroam run received 216 standalone type-1 material RPCs, but the former
  singleton `world_visual` transport exposed only 92 to the runtime. Those 92 all ended as `object_not_ready`
  (49) or `no_rw_object` (43), and all 80 inline CreateObject material lists were explicitly ignored. No material
  reached rendering. A transport snapshot or one-shot apply attempt is therefore not authoritative material state.
- `STATIC_037 + OPENMP_REF + INFERRED + TODO_VERIFY`: the replacement now keeps an authoritative, lazy material
  store in the network adapter, keyed by object ID, object generation, and material slot. Only slots actually
  received are allocated; a slot owns the complete normalized type-1/type-2 definition and its monotonically
  increasing material revision. Long text remains outside the frequently copied RPC snapshot. Destroy, generation
  replacement, disconnect, and adapter reset release the corresponding slot state. Material allocation/budget failure
  is fail-soft for object lifetime: the new Create generation still replaces the old object with an empty material
  state. The large diagnostic snapshot itself lives in static bridge storage rather than GTA's game-thread stack.
- `INFERRED + TODO_VERIFY`: ordered `SET_MATERIAL` object-ring entries are compact invalidation descriptors, not
  copies of the full material payload. They carry object ID, object generation, slot, source/type, and material
  revision. The runtime uses `samp_raknet_client_get_object_material` to fetch the latest complete definition from
  the authoritative slot, ignores duplicate/older revisions, and keeps unresolved revisions pending while the GTA
  object, RW object, source model, TXD, or texture is not resident.
- `INFERRED + TODO_VERIFY`: a Create event establishes a new generation and triggers a 16-slot authoritative
  resync after the local object slot exists; this recovers inline CreateObject materials without emitting one large
  event per material. RakPeer may dispatch more RPC handlers than the outer packet budget in one `Receive()`, so the
  compact 1024-entry ring is not authoritative. The adapter also retains the latest alive/Create state per object ID.
  If the object-event consumer detects a sequence gap/ring overwrite, it first reconciles missing Create/Destroy
  generations and then
  rescans the authoritative slots for every active or pending object generation instead of assuming the missing
  descriptors were non-material events. Generation equality prevents a late material revision from an earlier
  occupant of the same object ID from reaching the replacement object.
- `STATIC_037 + OPENMP_REF`: default material colour is ABGR on the wire and is copied as a raw DWORD into
  RenderWare's RGBA bytes. Zero means no colour override; alpha zero is not rewritten to opaque. Colour is applied
  independently of texture resolution. The transient draw path clears geometry flag `0x08`, sets `0x40`, and restores
  both afterward. Text foreground and background colours remain ARGB fields.
- `STATIC_037 + TODO_VERIFY`: type-2 definitions enter `CObject::SetMaterialText` at `samp.dll+0xA86B0` and are
  constructed lazily by `+0xA9EE0` through the text-texture manager at `+0x708B0`; `+0xA8ED0` releases generated
  textures for device loss. The replacement decodes and persists these definitions, but rendering still needs the
  original `samp.dll+0x74B50` D3D `SetTexture` substitution and a verified lost/reset-device lifecycle.
- `STATIC_037 + TODO_VERIFY`: model-zero material handling is still open. It must reproduce the dedicated cached
  `SAMP\\blanktex.txd` lifetime and must not be collapsed into either the colour-only `model < 0` case or the
  now-active positive-source-model streaming path.

Render-hook validation target: GTA SA 1.0 US `gta_sa.exe`, SHA256
`a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26`.

## Implementation Path

1. `PROBE_TRACE`: capture path-only original run with `samp_probe_asset_paths.flag`.
2. `STATIC_037`: identify original function-entry RVAs for IDE/IMG registration, not just file API callsites.
3. `GTA_REVERSED_REF`: implement a registration-only phase:
   - add `SAMP\CUSTOM.IMG`, `SAMP\SAMP.IMG`, and `SAMP\SAMPCOL.IMG` through `CStreaming::AddImageToList`;
   - parse `samp.IDE` / `custom.IDE` and create section-appropriate `CModelInfo` slots for object IDs with no existing `CModelInfo`;
   - set model name and texture dictionary metadata from IDE (`model_name`, `txd_name`);
   - run GTA-compatible directory registration for DFF/TXD/COL entries.
4. `TODO_VERIFY`: register archive entries without rendering first; validate `CModelInfo::ms_modelInfoPtrs[id] != 0` and `CTxdStore::FindTxdSlot(txd_name) != -1` for sampled custom IDs.
5. `TODO_VERIFY`: only then allow SA-MP custom object IDs through the existing object bridge. The bridge no longer has a canary whitelist, but it remains closed until `CModelInfo::ms_modelInfoPtrs[id] != 0`.

## Concrete Next Slice

### Switchable custom-object ModelInfo/render path

`MTA_REF + PROBE_TRACE + INFERRED + TODO_VERIFY`: the replacement accepts
`SAMPDLL_CUSTOM_OBJECT_RENDER_PATH=heap` through the process environment or
`sampdll.ini`. `heap` is the default and currently the only enabled path. It
skips AtomicModelInfo-store relocation, clones a validated atomic template into
one heap allocation per indexed custom `objs` model, clears its runtime
reference, collision and RenderWare ownership, and installs the pointer directly
in `CModelInfo::ms_modelInfoPtrs`. GTA still uses its ordinary streaming,
RenderWare and script-opcode object creation; this is not a Direct3D proxy path.
Synthetic atomic entries reset template-owned 2DFX/object-info/ownership fields
to GTA's `CBaseModelInfo::Init` baseline and apply the parsed IDE flags through
GTA `SetAtomicModelInfoFlags` (`gta_sa.exe` VA `0x5B3B20`). This includes
`DRAW_LAST` for alpha-sorted custom models such as `Tube50mGlass1` (18809).

The relocated-store `native` implementation remains compiled as a diagnostic
fallback, but its runtime selection is disabled while the heap path is being
qualified. An old `native`, `store`, or `0` setting therefore selects `heap` and
is reported as `native_disabled=1` in `samp_runtime.log`. The selection is cached
and therefore requires a game restart.

For an A/B run, keep server, route and object IDs identical and compare
`object_model_state` phases `pre_request`, `post_request`, `post_load`,
`post_create`, `pre_destroy`, and `post_destroy`. The rows include actual
`model_info_path`, ModelInfo/vtable/key, TXD slot/streaming row, `pColModel`,
RenderWare object and DFF streaming state. `object_lifecycle: create_end` and
`destroy_end` additionally record the GTA object/entity lifetime. Probe model
15417 only when the loaded IDE actually indexes it; the stock SA-MP high custom
range begins at 18631, so 18631 and later observed IDs are the authoritative
stock test set.

1. `STATIC_037 + PROBE_TRACE + TODO_VERIFY`: keep mapping the original early-init IDE/archive function RVAs so
   the replacement can move custom model registration from late object fallback toward the original startup path.
2. `PROBE_TRACE + TODO_VERIFY`: verify the default full bulk pass reports `considered=1433`, `skipped=0`, IDE-order
   registration, and Atomic store `13984->15417` before the first `archive_dir_batch`. Then rerun
   `/sampobjscan 11682 5000 3000`; after `11753`, model `18631` and later high IDs must show nonzero DFF `cd_size`,
   load state `1`, readable `rw_object`, and successful `create_opcode_end`.
3. `PROBE_TRACE + TODO_VERIFY`: if a stock Atomic still logs `streaming_dff_unmapped`, keep it pending and inspect
   bulk summary/order plus the first archive pass; do not raise the replay limit without new evidence.
4. `PROBE_TRACE + TODO_VERIFY`: with `SAMPDLL_CUSTOM_OBJECT_VISUAL_FALLBACK=1`, verify blocked custom objects log
   `object_visual_fallback: activate` and draw projected markers before adding real DFF/RenderWare loading.
5. `PROBE_TRACE + TODO_VERIFY`: run custom and original prefixes on the same Area51 script and diff
   object visibility plus `samp_asset_registration` / `object_exception` lines.
6. `TODO_VERIFY`: verify `CModelInfo::ms_modelInfoPtrs[id] != 0`, TXD slot, and DFF streaming state
   for sampled custom IDs after directory load.
7. `TODO_VERIFY`: implement COL registration/validation if world objects render but have missing or
   wrong collision.
8. `OPENMP_REF + PROBE_TRACE + TODO_VERIFY`: verify attached custom objects on original and replacement runs,
   especially vehicle/object parent ordering and player attachments sent through RPC 75.
9. **CLOSED / I.O.** (`STATIC_037 + OPENMP_REF + PROBE_TRACE`):
   `RemoveBuildingForPlayer` is implemented beyond decode-only, persists its
   rules and reapplies them while streaming. Exact R5 visual comparison is a
   non-blocking parity note rather than an open RPC gap.
10. `PROBE_TRACE + STATIC_037`: rerun SuperFreeroam and require matching `object-material-decode: queue` /
    authoritative-revision / `object_material: persist` totals, successful `object_material: resolved`, and at
    least one `object_material: render_apply`. Exercise both an inline-material Create resync and a forced object-ring
    gap; both must converge to the latest authoritative slot revisions without applying an older generation. Visually
    compare the glass, coloured-light, decking, wallpaper, and `cauldron1/alienliquid1` replacements seen in the prior
    RPC 84 payloads.

## Guardrails

- Do not pass a custom model to `RequestModel` until model info is present.
- Keep `SAMPCOL.IMG` reads out of the ASI trace unless explicitly enabled.
- Prefer archive/IDE registration before any Direct3D mesh fallback; the original behaves like a GTA asset integration layer, not just an immediate-mode renderer.
- Do not treat the compact material-event ring as authoritative. On Create or an event gap, recover complete slots
  from the adapter store and require an exact object-generation match.
- Do not consume a material revision merely because its GTA entity, RW object, TXD, or texture is temporarily absent;
  keep it pending and retry without permanently modifying shared `RpGeometry`.

## Source Links

- `GTA_REVERSED_REF`: https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Streaming.cpp
- `GTA_REVERSED_REF`: https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Models/ModelInfo.cpp
- `GTA_REVERSED_REF`: https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/TxdStore.cpp
- `INFERRED`: https://github.com/GTAmodding/III.VC.SA.LimitAdjuster/blob/master/src/limits/ModelInfo/AtomicModels.cpp
- `INFERRED`: https://github.com/GTAmodding/III.VC.SA.LimitAdjuster/blob/master/src/limits/ModelInfo/StoreAdjuster.hpp
- `INFERRED`: https://github.com/JuniorDjjr/CLEOPlus
- `INFERRED`: https://github.com/thelink2012/modloader/blob/master/src/plugins/gta3/std.stream/directory.cpp
- `INFERRED`: https://github.com/thelink2012/modloader/blob/master/src/plugins/gta3/std.stream/backend.cpp
- `INFERRED`: https://github.com/DK22Pac/plugin-sdk
