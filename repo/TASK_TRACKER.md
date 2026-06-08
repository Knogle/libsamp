# Task Tracker

Stand: 2026-06-08

Dieser Tracker beschreibt den Arbeitsstand ab dem aktuellen spielbaren
Meilenstein. Prioritaeten:

- `P0`: Blockiert Kernspielbarkeit oder Public-Repo-Qualitaet.
- `P1`: Wichtig fuer Kompatibilitaet und sichtbare Paritaet.
- `P2`: Stabilisierung, Tests, Qualitaet.
- `P3`: Komfort, Dokumentation, Cleanup.

Status:

- `[todo]`: Noch nicht begonnen.
- `[doing]`: In Arbeit.
- `[blocked]`: Blockiert durch fehlende Trace-/Source-Evidenz.
- `[verify]`: Implementiert, braucht Golden-Trace-/Gameplay-Check.
- `[done]`: Fertig und getestet.

## 1. Public Repo Hygiene

- [done] `P0` Projektname und Zielsetzung auf `libsamp` / `Libre-SAMP` als
  `samp.dll`-Drop-in-Replacement ausrichten.
- [done] `P0` Alte Crosswalk-/Planungsdateien entfernen, die nicht public-tauglich
  sind.
- [done] `P0` Lokale Referenzquellen aus oeffentlicher Namensgebung entfernen.
- [done] `P0` Doku auf Original-DLL-Reverse-Engineering, Golden-Traces und
  oeffentliche Referenzen umstellen.
- [todo] `P0` Index pruefen: keine proprietaeren Binaries, keine lokalen
  Arbeitsassets, keine privaten Prefix-Kopien.
- [todo] `P0` Lizenzdateien und Third-Party-Hinweise finalisieren.
- [done] `P1` README fuer ersten Public-Stand schreiben.
- [done] `P1` GitHub Actions baut public-taugliche Win32-Artefakte fuer
  `samp.dll` und ASI-Probe mit vendored Knogle/RakNet-Transportpfad.
- [todo] `P1` Build-/Runbook fuer Devbuild-Toolbox und Prefix-Deployment
  dokumentieren.
- [verify] `P1` Network-enabled CI-Build gegen Golden-Trace-Szenarien
  verifizieren, sobald public-taugliche Fixtures vorliegen.
- [todo] `P2` Trace-Sanitizer fuer Logs bauen oder dokumentieren.

## 2. ABI / DLL Surface

- [verify] `P0` Exports, Ordinals und PE-Profil gegen Original-DLL-Analyse
  pruefen. Evidence: `STATIC_037`.
- [verify] `P0` DllMain minimal halten und keine Exceptions ueber DLL-Grenzen
  leaken.
- [todo] `P0` ABI-Tracker um aktuelle Runtime-Hooks, Env Flags und Build-Artefakte
  ergaenzen.
- [todo] `P1` `static_assert` fuer binär relevante Structs ergaenzen:
  RPC payloads, TextDraw transmit structs, player/vehicle sync structs.
- [todo] `P1` Hook-Dokumentation je Patch pflegen: Modul/RVA, Originalbytes,
  Patchbytes, Patchlaenge, Restore-Pfad.

## 3. Boot / Lifecycle

- [done] `P0` Pre-connect Panorama, Textausgabe und 10s Delay implementiert.
  Evidence: `PROBE_TRACE`, `OBSERVED_037`.
- [done] `P0` ESC/Pause im Pre-connect haelt Progression an.
- [verify] `P0` Device-Selection-Resolution-Liste erweitert, inklusive
  2560x1440.
- [verify] `P1` Loading-Screen ueber `LOADSCS.txd`-Pfad eingebunden.
- [todo] `P1` Shutdown/Disconnect-Pfade vereinheitlichen: `/q`, `/quit`,
  Server-Disconnect, Game-Close.
- [todo] `P1` Reconnect-State sauber resetten: RPC tables, local player,
  vehicles, textdraws, labels, gangzones, objects, timers.

## 4. Netcode / RPC Coverage

- [done] `P0` RakNet-Basisconnect, Join, Chat und Dialogantworten funktionieren.
  Evidence: `OPENMP_REF`, `PROBE_TRACE`.
- [verify] `P0` RPC-Decoder-Inventory ist breit genug, um unbekannte RPCs als
  Stub mit Log zu erfassen.
- [todo] `P0` Alle servergetriggerten RPCs lokal als sichere Stubs mit Bounds
  Checks registrieren.
- [todo] `P0` Unknown-RPC-Policy: einmalige Chat-Meldung, lokale Logdetails,
  kein Crash.
- [todo] `P1` Bitstream-Fixtures fuer kritische RPCs:
  `SetSpawnInfo`, `RequestSpawn`, `Spawn`, `SetPlayerPos`,
  `PutPlayerInVehicle`, `CreateVehicle`, `ShowTextDraw`, `ShowDialog`,
  `CreateObject`, `SetObjectMaterial`, `SetPlayerTime`.
- [todo] `P1` Client-to-server sync pruefen: on-foot, in-vehicle, passenger,
  aim, damage, death, request class, request spawn.
- [todo] `P1` Disconnect/timeout untersuchen: Keepalive, ack cadence,
  player-sync cadence, vehicle-sync cadence.
- [todo] `P2` Fuzz-/Bounds-Test fuer RPC-Leser ohne Netzwerk.

## 5. Spawn / Player State

- [done] `P0` Spawn an korrekter Position und Teleport mit
  `RefreshStreamingAt`/`RestartIfWastedAt` umgesetzt.
- [done] `P0` Kamera und Maussteuerung nach Spawn funktionsfaehig.
- [verify] `P0` Death/Respawn verlaesst keinen detached State mehr.
- [todo] `P0` Remote-Player-Sync sichtbar machen: create, skin/model, position,
  movement, vehicle enter/exit.
- [todo] `P0` Player-Blips fuer Remote-Spieler auf Radar und Map.
- [todo] `P1` Player score/ping/name/color/team fuer TAB-Liste und Blips
  synchronisieren.
- [todo] `P1` `SetPlayerArmour`, `SetPlayerArmedWeapon`, `ResetPlayerWeapons`,
  `ResetPlayerMoney` vollstaendig verifizieren.
- [todo] `P1` `ApplyAnimation` korrekt auf lokale und remote Peds abbilden.
- [todo] `P2` Ped audio, CJ-Stimmen, fight-style, special actions und armed
  weapon edge cases pruefen.

## 6. Vehicles

- [done] `P0` Server-Vehicles werden erzeugt, geladen und befahrbar.
- [done] `P0` Vehicle-Streaming stabilisiert, keine direkten Crashes bei vielen
  Vehicles.
- [verify] `P0` Vehicle-Blips: leere Vehicles sollen sichtbar sein, aber Farbe
  und Alpha muessen korrekt sein.
- [todo] `P0` Remote-Spieler in Vehicles synchronisieren.
- [todo] `P1` Vehicle health, color, siren, paintjob, components, numberplate
  anwenden.
- [todo] `P1` Lock-State/Doors/Engine/Lights/Objective Marker korrekt abbilden.
- [todo] `P1` Vehicle respawn/despawn sauber aus GTA-Pools entfernen.
- [todo] `P2` Streaming-Budget und LOD-Strategie dokumentieren.

## 7. Objects / Custom Assets

- [doing] `P0` Normale GTA-SA-Objekte ueber GTA-Pipeline erzeugen.
- [doing] `P0` Unsupported custom models nicht crashen, sondern loggen.
- [todo] `P0` SA-MP-Custom-Object-Asset-Pipeline finalisieren:
  IMG/TXD/DFF finden, Modell-ID-Mapping, Load/Unload, Materialbindung.
- [todo] `P0` `CreateObject`, `DestroyObject`, `MoveObject`, `StopObject`,
  `AttachObjectToPlayer`, `AttachObjectToVehicle` defensiv implementieren.
- [todo] `P1` `SetObjectMaterial` und `SetObjectMaterialText` mit Alpha,
  Font, Alignment und Texture-Dictionary-Unterstuetzung.
- [todo] `P1` Object-Streaming-Radius und Priorisierung gegen Golden-Traces
  pruefen.
- [todo] `P2` Crash-fixture fuer unsupported model IDs und fehlende Assets.

## 8. TextDraw Renderer

- [verify] `P0` Basis-Rendering mit Text, Farben, Schatten, Outline und
  Transparenz sichtbar.
- [doing] `P0` Leere Box-/Sprite-TextDraws korrekt rendern.
- [todo] `P0` Klickbare TextDraws: Hover, Cursor, SelectTextDraw-Farbe,
  Click-RPC an Server.
- [todo] `P1` GTA-CFont-Paritaet: Spacing, proportional fonts, underscores,
  color tokens, line height, align left/center/right.
- [todo] `P1` Sprite-/ModelPreview-TextDraws: Texture name lookup,
  model loading, camera rotation, zoom.
- [todo] `P1` Resolution-skalierte Koordinaten finalisieren:
  640x448/480 logical space, widescreen, 2560x1440.
- [todo] `P2` Golden-Trace-Screenshot-Vergleich fuer TextDraw-Szenen.

## 9. Chat / Input

- [done] `P0` Chat empfangen und senden funktioniert.
- [verify] `P0` `/q` und `/quit` verdrahtet.
- [todo] `P1` Chat-Farben und Embedded-Color-Tokens final angleichen.
- [todo] `P1` Chat-Input-Box: 80% transparente schwarze Box ab `>` dynamisch
  bis Eingabeende, 1px vertikale Korrektur beibehalten.
- [todo] `P1` Command/Text spacing fixen: Leerzeichen nach Commands und zwischen
  TextDraw-Woertern darf nicht verloren gehen.
- [todo] `P1` Chat-History, Cursor, Backspace, Paste, Enter/Escape, max length,
  non-ASCII handling.
- [todo] `P2` Optional debug-only chat dump fuer serverseitige Echo-Tests.

## 10. Dialoge / Menus / Mouse Mode

- [done] `P0` Ingame-Dialoge statt Win32-Dialoge im Normalmodus.
- [done] `P0` Win32-Dialog nur noch fuer expliziten Debug-Modus vorgesehen.
- [verify] `P0` Mausmodus fuer Dialoge und TextDraw-Selection.
- [todo] `P1` Dialog Styles vollstaendig:
  message box, input, password, list, tablist, headers.
- [todo] `P1` Dialog layout: Scrollbar, selection, OK/Cancel, text wrapping,
  color tags.
- [todo] `P2` Dialog-Input-Validierung und SendDialogResponse-Fixtures.

## 11. HUD / Frontend / Pause

- [done] `P0` TAB zeigt SA-MP-Spielerliste statt Singleplayer-Stats.
- [done] `P0` ESC/Pause bleibt offen und wird nicht sofort geschlossen.
- [verify] `P1` Scoreboard lokaler Spieler plus Remote-Spieler mit Score/Ping.
- [todo] `P1` Scoreboard sort/order, colors, ping updates, max rows,
  scrollbar.
- [todo] `P1` HUD visibility RPCs: radar, clock, money, health/armour,
  wanted, zone names.
- [todo] `P2` F8 screenshots weiter angleichen: Dateinamen, Chatmeldung,
  Fehlerfaelle.

## 12. World / Visual State

- [done] `P0` Motion blur/bloom beim schnellen Fahren deaktiviert.
- [done] `P0` World-Time wird serverseitig angewendet.
- [verify] `P0` Weather/Time nicht durch lokalen Singleplayer-Timer
  zurueckgedreht.
- [todo] `P1` World-Streaming bei schneller Fahrt und Teleports stabilisieren:
  keine fehlenden Blocks, keine HUD-Aussetzer.
- [todo] `P1` `RemoveBuildingForPlayer` implementieren und gegen serverseitige
  Map-Szenen testen.
- [todo] `P1` GangZones: create/destroy/show/hide/flash/stop flash.
- [todo] `P2` Camera RPCs: interpolate, attach, spectate, reset.

## 13. Audio

- [verify] `P1` `PlayerPlaySound`/`PlaySound` implementiert und gegen Run pruefen.
- [todo] `P1` `PlayAudioStreamForPlayer` und `StopAudioStreamForPlayer`.
- [todo] `P2` Audio edge cases: stop on disconnect, stop on spawn reset,
  duplicate stream handling, volume/range.

## 14. 3D Text / Labels / Pickups / Checkpoints

- [todo] `P1` `Create3DTextLabel`, attach to player/vehicle, update, delete.
- [todo] `P1` Pickups: create/destroy, model loading, pickup notification.
- [todo] `P1` Checkpoints und race checkpoints: show/hide, size, target marker.
- [todo] `P2` Label distance culling und LOS/behind-camera Verhalten.

## 15. Remote Players

- [doing] `P0` Remote player create/show path.
- [todo] `P0` Remote on-foot sync: position, rotation, velocity, animation,
  weapon, keys.
- [todo] `P0` Remote in-vehicle sync: driver/passenger, seat, vehicle transform,
  health.
- [todo] `P1` Remote aim/damage/death events.
- [todo] `P1` Remote name tags und health bars.
- [todo] `P1` Remote player colors im Chat, TAB, Blips und Nametags.
- [todo] `P2` Interpolation/lag smoothing statt harte Teleports.

## 16. Server Testbed

- [todo] `P0` Bare open.mp Testserver-Szenarien versionieren:
  spawn, teleport, vehicles, textdraw, dialogs, remote player, object stress.
- [todo] `P1` Scripted scenario runner: Client startet, Server sendet definierte
  RPC-Sequenzen, Logs werden normalisiert.
- [todo] `P1` Zwei-Client-Szenario fuer Remote-Player-Sync.
- [todo] `P2` Long-run soak test: 30 min drive, teleports, death, reconnect.

## 17. Debugging / Tooling

- [done] `P0` ASI-Probe und Runtime-Logs fuer Golden-Traces nutzbar.
- [todo] `P1` Probe um Remote-Player-/Vehicle-/Object-Pool-Snapshots erweitern.
- [todo] `P1` Packet/RPC trace diff tool stabilisieren.
- [todo] `P1` Log clear/build/deploy/run Kurzkommandos dokumentieren.
- [todo] `P2` Symbol-/RVA-Map automatisch aus Build und Analyse erzeugen.

## 18. Sicherheits- und Kompatibilitaetsgrenzen

- [todo] `P0` Keine Cheat-, Anti-Cheat-Bypass- oder Public-Server-Missbrauchs-
  Features dokumentieren oder implementieren.
- [todo] `P0` Serverdaten immer bounds-checken.
- [todo] `P1` Unknown/oversized payloads nie crashen lassen.
- [todo] `P1` Feature Flags fuer riskante Hooks beibehalten.
- [todo] `P2` Compatibility Notes fuer bekannte nicht unterstuetzte Serverfeatures.

## Naechste sinnvolle Arbeitsreihenfolge

1. `P0` Public-Repo Hygiene final pruefen und README/Lizenz setzen.
2. `P0` Remote-Player-Sync sichtbar machen inklusive Blips.
3. `P0` Unknown-RPC-Stubs vollstaendig und crashfrei machen.
4. `P0` Custom-Object-Pipeline stabilisieren, zuerst GTA-Objekte, dann
   SA-MP-Custom-Assets.
5. `P1` TextDraw Click/Selection und ModelPreview.
6. `P1` Scoreboard mit lokalem Spieler, Remote-Spielern, Ping/Score.
7. `P1` Long-run Disconnect/Resync systematisch reproduzieren.

## Aktuelle offene Risiken

- `INFERRED`: Einige Runtime-Hooks sind anhand plausibler Original-DLL-Traces
  rekonstruiert, aber noch nicht vollstaendig gegen mehrere Szenarien validiert.
- `TODO_VERIFY`: Custom object materials und model previews koennen noch falsche
  Asset- oder Alpha-Semantik haben.
- `TODO_VERIFY`: Remote player sync ist noch nicht als vollstaendig spielbar
  belegt.
- `TODO_VERIFY`: Hohe Vehicle-/Object-Dichte kann weiterhin Streaming- oder
  Pool-Probleme ausloesen.
- `TODO_VERIFY`: TextDraw-Koordinaten und CFont-Spacings sind sichtbar besser,
  aber noch nicht 1:1.
