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

## Next-run acceptance markers

- `object: class_scene_ready` before all pre-spawn `object: create` markers;
- IDs 102-139 created before `spawn_finalize_begin`;
- their explicit destroy RPCs produce `object: destroy`, not
  `destroy_pending`;
- reused IDs create a new generation after the spawn hold;
- `textdraw_gta_font: prespawn_ready` and
  `textdraw_gta_font_gameprocess: drawing enabled ... hud=0` before spawn;
- no `create_opcode ... ok=0`, streaming/model errors,
  `exception_filter`, freeze, or duplicate TextDraw rendering.

Font 5 remains a proxy in the replacement. The next original run should use
the dedicated PE-identity-proxy/prologue-guarded Font 5 probe before
implementing the real model-preview renderer. Record and verify the documented
original DLL hash separately before that run.
