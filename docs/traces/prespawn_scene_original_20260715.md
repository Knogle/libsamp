# Original 0.3.7 Pre-Spawn Scene - 2026-07-15

## Capture identity

- Client: original SA-MP 0.3.7-R5 reference prefix
  `/home/chairman/Games/san-andreas-multiplayer-legacy-legacy`
- `samp.dll` SHA256:
  `b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2`
- Server/scenario: Ultimate Freeroam 4.2, initial class/mode selection
- Screenshot `14-29-43` SHA256:
  `8e1d2fd4b10082e7fe61e90df5f05b83d3f064b5ab8ad88f5054846b17141b83`
- Screenshot `14-29-50` SHA256:
  `4dfb248eeda7fe5f34d38f2d81af673bcea30743c777e4e1dccbac41018f3535`

The screenshots remain outside the repository under the original capture
directory; their hashes identify the observations without redistributing game
assets.

## Observed sequence

`OBSERVED_037`:

1. At `14:29:43`, before spawn, the server-selected scene is already visible:
   the colored floor, large SFR letters, aircraft, local class ped, and bottom
   class-selection controls are present.
2. At `14:29:50`, the same world remains loaded and the mode-selection
   TextDraw layer appears over it.
3. Thus world-object availability is not gated on successful Spawn, and the
   normal TextDraw phase can begin while the gameplay HUD is still disabled.

`PROBE_TRACE` from the comparable replacement run:

- the scene arrives as unattached object IDs 102 through 139 around `z=2500`;
- the local class scene is prepared and the ped teleport succeeds before
  `spawn_finalize_begin`;
- the old replacement gate kept those decoded objects pending until spawn;
- the server later sends explicit destroys for the class-scene generation
  before reusing the IDs for gameplay objects.

## Compatibility boundary implemented

`OBSERVED_037` + `PROBE_TRACE` + `INFERRED` + `TODO_VERIFY`:

- class-scene readiness is bound to the exact current `SetPlayerPos` sequence
  and published only after scene preparation and successful local-ped
  teleport;
- a new position sequence invalidates readiness before object events are
  flushed;
- only unattached world objects may instantiate in this phase;
- custom-asset registration and object-material preparation use the same
  scene/slot gate, so the SFR material overrides can resolve without admitting
  attached objects;
- vehicles and attached objects remain spawn-only;
- the post-spawn object hold is armed before spawn readiness and asset drains;
- early normal TextDraw CFont rendering is restricted to the
  register-preserving GameProcess phase; EndScene/graphics remain gated.

## Purple pre-class wallpaper

`OBSERVED_037` + `PROBE_TRACE`:

- the large purple layer shown before the mode/class selection is not the GTA
  loading-screen image and not a normal TextDraw box;
- the server sends global TextDraw slot 12 as Font 5 with model `19129`, text
  `LD_SPAC:white`, position `(-1.405576, -49.583057)`, size
  `(673.909424, 551.833313)`, rotation `(90, 0, 100)`, zoom `0`, and black
  preview background;
- the original DLL identified above prepares that custom model from caller
  `samp.dll+0x0001E888` and draws the resulting preview from
  `samp.dll+0x0001E856` before the class scene is ready;
- the 2026-07-16 replacement run received the same slot and queued model
  `19129`, but logged `samp_custom_model_unregistered`; the server hid slots
  12 through 16 before the object-scene gate finally drained the custom-model
  registrations.

The replacement now tags Font 5 registration requests when the ShowTextDraw
snapshot is applied. The existing object-bridge lifecycle may drain that
request while `entry=9` and `game_started=0`; the HUD/TextDraw render hook does
not perform streaming or RenderWare destruction. This exception is active only
while an unapplied Font 5 custom-model request exists. World-object creation
remains under the class-scene/spawn gate.

The 2026-07-16 replacement run with DLL SHA256
`be514e9b1e1dd6d8cf4df246d8bb9c714ec235274fb81898deb83a1a916d56d4`
confirmed registration before hide, generic-path preparation with transmitted
zoom `0.000`, an unclamped overscan draw in the GameProcess TextDraw pass, and
the expected purple `DanceFloor2` wallpaper below the normal foreground slots.

## Next-run acceptance markers

- `object: class_scene_ready` before all pre-spawn `object: create` markers;
- IDs 102-139 created before `spawn_finalize_begin`;
- their explicit destroy RPCs produce `object: destroy`, not
  `destroy_pending`;
- reused IDs create a new generation after the spawn hold;
- `textdraw_gta_font: prespawn_ready` and
  `textdraw_gta_font_gameprocess: drawing enabled ... hud=0` before spawn;
- `samp_asset_registration: drain source=object_bridge` (or
  `source=object_bridge_idle`) with `prespawn_preview=1` before
  `textdraw: hide ... id=12`;
- `textdraw_preview: prepared ... model=19129` while slot 12 is still active;
- no `create_opcode ... ok=0`, streaming/model errors,
  `exception_filter`, freeze, or duplicate TextDraw rendering.
