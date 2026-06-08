# RE Evidence Guide

Diese Regeln halten neue Implementierungen nachvollziehbar und public-tauglich.

## Evidence-Tags

- `OBSERVED_037`: Direkt mit originaler SA-MP 0.3.7-DLL beobachtet.
- `PROBE_TRACE`: Durch ASI-Probe, Golden-Trace oder Runtime-Log belegt.
- `STATIC_037`: Statisch aus originaler 0.3.7-DLL analysiert.
- `OPENMP_REF`: Aus open.mp-Protokoll, Dokumentation oder Serververhalten
  abgeleitet.
- `GTA_REVERSED_REF`: Aus gta-reversed fuer GTA-SA Engine-Integration abgeleitet.
- `SAMPFUNCS_REF`: Aus SAMPFUNCS/Modding-Referenzen fuer API-/Hook-Verhalten
  abgeleitet.
- `INFERRED`: Plausibel, aber noch nicht belegt.
- `TODO_VERIFY`: Muss gegen Original-DLL oder Golden-Trace validiert werden.

## Wann welcher Tag?

- ABI, Struct-Layouts, RVAs, Calling Conventions:
  `STATIC_037` oder `OBSERVED_037`.
- Runtime-Reihenfolge, Game-State, Hooks:
  `PROBE_TRACE` plus optional `OBSERVED_037`.
- RPC-Semantik und Serverantworten:
  `OPENMP_REF` plus eigene Trace-ID.
- GTA-Pools, CFont, CCamera, CPed, CVehicle:
  `GTA_REVERSED_REF`, aber nur defensiv einsetzen.
- Heuristiken und Fallbacks:
  `INFERRED` und `TODO_VERIFY`.

## Trace-Regeln

- Original-Run und Replacement-Run vergleichbar halten.
- Timestamps, Pointer, Modulbasis und Zufallswerte beim Diff normalisieren.
- Wichtigste Szenarien separat speichern:
  - Load/Init
  - Connect/Handshake
  - Spawn/Respawn
  - Chat/Dialog/TextDraw
  - Vehicle State
  - Remote Player State
  - Object/Material State
  - Disconnect/Shutdown
- Beim Uebernehmen in Doku nur normalisierte Ausschnitte verwenden.

## Kommentar-Beispiel

```c
// OBSERVED_037 + PROBE_TRACE:
// Original 0.3.7-R5 applies the local spawn camera reset after server spawn
// acceptance and before the first steady player sync. Keep this order because
// earlier RefreshStreamingAt calls can leave the client near stale world cells.
```

## Was nicht einchecken

- Proprietaere DLLs, EXEs, TXDs, IMG-Dateien oder extrahierte Assets.
- Roh-Dumps mit privaten Prefix-Pfaden.
- Laengere proprietaere Disassembly-/Pseudocode-Bloecke.
- Spekulative Offsets ohne `TODO_VERIFY`.
