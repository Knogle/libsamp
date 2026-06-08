# libsamp / Libre-SAMP - Public Working Folder

Dieser Ordner ist der saubere Einstiegspunkt fuer die oeffentliche Repo-Aufbereitung.
Er enthaelt keine proprietaeren Binaries, keine lokalen Referenzpfade und keine
Verweise auf nicht-oeffentliche Quellbestaende.

## Projektziel

libsamp, ausgeschrieben Libre-SAMP, entwickelt ein moeglichst kompatibles
Drop-in-Replacement fuer die SA-MP 0.3.7 `samp.dll`, das mit
0.3.7-kompatiblen Servern und open.mp zusammenarbeitet.

Der normale SA-MP-Installer wird vorerst weiterhin gebraucht, um die
Client-Umgebung, Assets und begleitenden Dateien bereitzustellen. Dieses
Repository liefert keine proprietaeren Client- oder GTA-SA-Dateien aus. Das
konkrete Ziel dieses Projekts ist die austauschbare `samp.dll` fuer eine
vorhandene, lokal installierte Client-Umgebung.

Die aktuelle Implementierung basiert auf Reverse Engineering der originalen
0.3.7-DLL, Golden-Traces, ASI-Probe-Logs, open.mp-Protokollsemantik,
gta-reversed-Engine-Referenzen und projektinternen Laufzeitbeobachtungen.

## Oeffentlicher Arbeitsmodus

- Kleine, reviewbare Aenderungen.
- ABI, Exports, Calling Conventions, Struct-Layouts und RPC-Layouts stabil halten.
- Unklare Semantik als Stub mit Log markieren, nicht spekulativ nachbauen.
- Jede technische Aussage mit Evidence-Tags kennzeichnen, wo sie nicht direkt aus
  dem Code ersichtlich ist.
- Keine proprietaeren Assets oder Binaries einchecken.

## Wichtige Dateien

- `TASK_TRACKER.md`: Umfassender Task-Tracker ab aktuellem Meilenstein.
- `PUBLICATION_CHECKLIST.md`: Checkliste fuer den ersten Public-Repo-Stand.
- `RE_EVIDENCE_GUIDE.md`: Evidence- und Trace-Regeln fuer zukuenftige Arbeit.

## Aktueller Meilenstein

Der Client erreicht inzwischen einen spielbaren Grundzustand:

- Connect/Join/Spawn gegen open.mp-kompatiblen Server.
- Chat senden und empfangen.
- Ingame-Dialoge mit Mausmodus.
- TAB-Spielerliste als SA-MP-Overlay statt Singleplayer-Stats.
- Vehicles werden serverseitig erzeugt, angezeigt und befahrbar.
- Spawn/Teleport-Streaming landet an plausiblen Positionen.
- World-Time und Basis-HUD werden serverseitig beeinflusst.
- TextDraws werden sichtbar, inklusive Transparenzverbesserungen.
- Custom-Object-Pipeline ist begonnen, aber noch nicht korrekt vollstaendig.

Die wichtigsten Restthemen sind Remote-Player-Sync, Object-/Material-Pipeline,
vollstaendige RPC-Abdeckung, TextDraw-Paritaet, Disconnect-/Resync-Stabilitaet
und systematische Golden-Trace-Vergleiche.
