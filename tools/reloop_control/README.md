# ReLoop control ASI

`reloop_control.asi` is a test-only input and telemetry bridge shared by the
original and replacement prefixes. It binds TCP only to `127.0.0.1:18737` and
accepts newline-delimited JSON carrying the fixed token `reloop-local-v1`.

The API exposes `ping`, `state`, `focus`, `key`, `char`, and `mouse`. `state`
samples the GTA HUD/radar/camera globals and the five bytes at
`gta_sa.exe+0x141df5`; it never writes game memory. Input commands are normal
Win32 window/input events so both DLLs receive the same stimulus.

Build with `build_win32.sh`, deploy the resulting ASI into both GTA roots, then
run `python3 tools/reloop/control_client.py scenario --output <file>` while the
client is connected. This deliberately uses a tiny dependency-free loopback
protocol instead of WebSocket framing; it is easier to make deterministic on
Wine and can be wrapped by WebSocket later without changing the command model.
