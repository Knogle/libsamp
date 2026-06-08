# Notices

Libre-SAMP (`libsamp`) is licensed under the MIT License. See
[LICENSE](LICENSE).

## Project Assets

The tracked loading-screen development assets are generated for this project
and are covered by the repository MIT License:

- `loadingscreen.jpg`
- `assets/LOADSCS.txd`

If these files are replaced in the future, the replacement source and license
must be documented before it is committed.

## Third-Party Components

### Knogle/RakNet submodule

Path: `third_party/raknet-knogle`

Source: <https://github.com/Knogle/RakNet>

This submodule contains a modified RakNet 2.52 codebase used as the
SA-MP/open.mp-oriented transport source. Its own README states that it is not
taken from leaked SA-MP source code. The historical RakNet notice in
`third_party/raknet-knogle/readme.txt` identifies:

- RakNet 2.52
- Copyright 2002-2005 Kevin Jenkins
- Jenkins Software / Rakkarsoft licensing notice

The repository MIT License does not relicense third-party code inside
submodules. Keep upstream notices intact when redistributing source or binary
artifacts.

The submodule also includes `SpeexLicense.txt`; preserve that notice if any
voice/speex-related code is distributed or enabled.

## Non-Included Software And Assets

This repository does not include:

- GTA San Andreas binaries or assets.
- Original SA-MP client binaries.
- Original SA-MP installer contents.
- Proprietary game, launcher, server, or client assets.

Runtime testing still requires a legitimate local GTA San Andreas installation
and the SA-MP client environment installed separately.
