# Publication Checklist

Diese Checkliste beschreibt den minimal sauberen Stand, bevor das Repository
oeffentlich sichtbar gemacht wird.

## P0 - Muss vor Public erledigt sein

- [ ] Keine proprietaeren Binaries oder Assets im Index.
- [ ] Keine lokalen Prefix-Pfade, privaten Nutzernamen oder absoluten Home-Pfade in
      dokumentierten Beispielkommandos.
- [ ] Keine nicht-oeffentlichen Referenzquellen im Repository.
- [ ] Keine alten Referenzpfadnamen oder nicht mehr genutzten Crosswalk-Dateien.
- [ ] `.gitignore` deckt lokale Traces, Builds, Prefix-Kopien, ZIPs und
      Arbeitsassets ab.
- [ ] README benennt klar: Reverse Engineering der originalen 0.3.7-DLL,
      Golden-Traces, open.mp, gta-reversed und eigene Probe-Logs als Quellen.
- [ ] Build-Anleitung beschreibt reproduzierbar den Devbuild-Pfad.
- [ ] Runtime-Debug-Logs sind standardmaessig begrenzt und enthalten keine
      sensitiven Pfade, sofern sie nicht explizit fuer lokale Analyse aktiviert
      werden.
- [ ] Lizenzstatus klaeren: eigener Code, eingebundene Third-Party-Komponenten,
      generierte Daten und Assets getrennt dokumentieren.

## P1 - Stark empfohlen

- [ ] `docs/re/` auf aktuellen Stand bringen: Boot, Netcode, Spawn, TextDraw,
      Dialog, Vehicle, Remote Player, Object Pipeline.
- [ ] `docs/traces/` mit kurzen, normalisierten Trace-Zusammenfassungen befuellen.
- [ ] Golden-Trace-Namen standardisieren:
      `load_init`, `connect_handshake`, `spawn`, `chat_dialog_textdraw`,
      `vehicle_state`, `remote_player_state`, `object_state`, `disconnect`.
- [ ] Kompatibilitaetsmatrix fuer getestete Server und Szenarien anlegen.
- [ ] Bekannte Risiken im README verlinken statt in Chatverlauf/Wiki zu belassen.
- [ ] Build-Artefakte klar von Quellcode trennen.

## P2 - Nach Public

- [ ] CI fuer Linux host tools und Win32 cross build.
- [ ] Smoke-Test fuer RakNet/RPC-Decoder mit Fixture-Bitstreams.
- [ ] Smoke-Test fuer TextDraw-Parser und Color-/Alignment-Konversion.
- [ ] Lint/format fuer C/C++ nur auf neuem Code aktivieren, keine
      Massenformatierung.
- [ ] Optionaler Trace-Sanitizer fuer Logs vor dem Einchecken.

## Public Messaging

Empfohlene Kurzbeschreibung:

> Clean-room-oriented SA-MP 0.3.7 `samp.dll` compatibility rebuild based on
> runtime traces, original DLL reverse engineering, open.mp protocol behaviour
> and GTA-SA engine references.

Nicht versprechen:

- Keine vollstaendige 1:1-Paritaet.
- Keine Kompatibilitaet mit allen Mods/Scripten.
- Keine Anti-Cheat-/Bypass-Faehigkeiten.
- Keine proprietaeren Daten im Repository.
