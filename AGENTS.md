# AGENTS.md — SA-MP 0.3.7 `samp.dll` RE Kurzvariante

## Ziel

Dieses Projekt entwickelt ein möglichst kompatibles Drop-in-Replacement für die SA-MP 0.3.7 `samp.dll`.

Modernisierungen etc. wenn sinnvoll gerne machen. Es soll kompatibel jedoch mit dem 0.3.7 Server bzw. open.mp Server bleiben.

## Arbeitsweise

* Arbeite konservativ und in kleinen, reviewbaren Änderungen.
* Vor Implementierung zuerst vorhandene RE-Doku, Traces, Probe-Logs und relevante Quellen prüfen.
* Direkte Beobachtungen der originalen 0.3.7-DLL schlagen alle anderen Quellen.
* Jede Annahme markieren: beobachtet, abgeleitet oder noch zu prüfen.
* Keine öffentlichen APIs, Exports, Calling Conventions, Struct-Layouts oder Protokollformate ändern, außer explizit begründet.
* Wenn Verhalten unklar ist: lieber Stub + Log + `TODO_VERIFY` als falsche Semantik.
* Antworte dem Maintainer auf Deutsch.

## Quellenpriorität

1. **Originale SA-MP 0.3.7 `samp.dll`**

   * Exports, Imports, Ordinals, RVAs, Calling Conventions, Speicherlayouts, Runtime-Verhalten.

2. **ASI-Probe / Golden Traces**

   * Memory-Inhalte, Game States, Hook-Events, Netzwerk-/RPC-Beobachtungen.


3. **Projektinterne Doku und bisherige Implementierung**

   * `docs/re/`, `docs/traces/`, `symbols/`, `tests/`, vorhandene Header/Code.

4. **open.mp**

   * API-/Semantik-/Kompatibilitätsreferenz:
   * https://github.com/openmultiplayer/open.mp
   * https://open.mp/docs

5. **gta-reversed**

   * GTA-SA-Engine-Referenz für Klassen, Hooks, Pools, Render/Ped/Vehicle/Camera:
   * https://github.com/gta-reversed/gta-reversed

6. **Ultimate ASI Loader**

   * Referenz für ASI-Loading, Plugin-Umgebung und Debugging:
   * https://github.com/ThirteenAG/Ultimate-ASI-Loader/tree/master

7. **SAMPFUNCS**

   * SAMP Functions, russischer Modding, SDK Guide etc., liegt lokal in sampfuncs, dient dazu um Mods in SAMP zu machen, z.B. Cheats, hilft uns aber ggf. weiter

8. **MTA:SA**

   * Niedrige Priorität, nur konzeptionell verwenden:
   * https://github.com/multitheftauto/mtasa-blue

## Evidenz-Tags

Nutze diese Tags in Kommentaren, Doku oder Commit-Kontext:

* `OBSERVED_037` — direkt an originaler 0.3.7-DLL beobachtet.
* `PROBE_TRACE` — durch ASI-Probe/Runtime-Log belegt.
* `STATIC_037` — statisch aus originaler DLL analysiert.
* `OPENMP_REF` — aus open.mp abgeleitet.
* `GTA_REVERSED_REF` — aus gta-reversed abgeleitet.
* `MTA_REF` — aus MTA abgeleitet, nur low priority.
* `INFERRED` — plausible, aber unbewiesene Annahme.
* `TODO_VERIFY` — muss noch gegen 0.3.7 validiert werden.

Beispiel:

```cpp
// OBSERVED_037 + PROBE_TRACE:
// samp.dll+0x00123456, SHA256=<hash>, run=<trace-name>
// Called after local player spawn, before first chat render.
```

## ABI- und Layout-Regeln

* Immer RVAs statt absoluter Adressen dokumentieren: `samp.dll+0x...`.
* Zu wichtigen Aussagen Hash/Build/Version notieren.
* Für binär relevante Structs `sizeof` und `offsetof` per `static_assert` absichern.
* Calling Conventions explizit halten.
* Keine Exceptions über DLL-/C-/WinAPI-Grenzen leaken.
* Rückgabewerte von Allocations, WinAPI, Datei-IO und Netzwerkfunktionen prüfen.

## ASI-Probe und Traces

* Probe-Logs sind primäre Evidenz.
* Original-Run und Replacement-Run vergleichbar halten.
* Pointer, Modulbasis, Timestamps und Zufallswerte beim Diff normalisieren.
* Wichtige Szenarien als Golden Traces dokumentieren:

  * Load/Init
  * Connect/Handshake
  * Spawn
  * Chat/Dialog/TextDraw
  * Vehicle/Ped/Object State
  * Disconnect/Shutdown

## Hooking und Loader

* `DllMain` minimal halten.
* Hooks nur setzen, wenn Zielbytes/Version validiert sind.
* Hook-Doku muss enthalten:

  * Modul + RVA
  * Originalbytes
  * Patchbytes
  * Patchlänge
  * Restore-Pfad
* `VirtualProtect`, Instruction-Cache-Flush und Restore sauber kapseln.
* Keine fremden Hooks blind überschreiben.

## Netcode und Protokoll

* Packet-/RPC-IDs, Bitstream-Layouts, String-Encoding, Limits und Reihenfolge gegen 0.3.7-Traces validieren.
* Serverdaten nie vertrauen.
* Alle Reads bounds-checken.
* open.mp als Semantikreferenz nutzen, aber 0.3.7-Beobachtung entscheidet.

## GTA-SA-Integration

* GTA-SA-Adressen immer mit Build/Hash/RVA dokumentieren.
* gta-reversed als Engine-Referenz nutzen, aber nichts ungeprüft übernehmen.
* Game-State-Zugriffe defensiv machen: Loading Screen, Pause, Disconnect, Respawn, Nullpointer und World-Wechsel beachten.

## Was vermeiden?

* Keine Cheats, Anti-Cheat-Umgehung, Ban-Evasion oder Public-Server-Missbrauch.
* Keine proprietären Binaries oder Assets einchecken.
* Keine großen Pseudocode-Blöcke aus proprietärer Disassembly übernehmen.
* Keine Massenformatierung.
* Keine unnötigen Dependencies.
* Keine spekulativen Offsets als Fakten darstellen.
* Keine Legacy-Quirks „fixen“, ohne Kompatibilitätsmodus oder Begründung.

## Abschlussformat

Nach Änderungen kurz melden:

```md
## Ergebnis
- Geändert: ...
- Evidenz: OBSERVED_037 / PROBE_TRACE / ...
- Tests/Checks: ...
- Kompatibilitätsrisiken: ...
- Offene Punkte: ...
```

Wenn nichts getestet wurde, ehrlich sagen.
::: 
