#!/usr/bin/env python3
"""Host client and scripted UI interaction scenario for reloop_control.asi."""

from __future__ import annotations

import argparse
import json
import socket
import time
from pathlib import Path
from typing import Any

TOKEN = "reloop-local-v1"


class ControlClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 18737, timeout: float = 3.0):
        self.socket = socket.create_connection((host, port), timeout=timeout)
        self.stream = self.socket.makefile("rwb", buffering=0)

    def close(self) -> None:
        self.stream.close()
        self.socket.close()

    def command(self, cmd: str, **fields: Any) -> dict[str, Any]:
        payload = {"token": TOKEN, "cmd": cmd, **fields}
        self.stream.write(json.dumps(payload, separators=(",", ":")).encode() + b"\n")
        line = self.stream.readline()
        if not line:
            raise ConnectionError("reloop_control closed the connection")
        response = json.loads(line)
        if not response.get("ok"):
            raise RuntimeError(f"control command failed: {response}")
        return response

    def key(self, vk: int, action: str) -> dict[str, Any]:
        return self.command("key", vk=vk, action=action)

    def text(self, value: str) -> None:
        for character in value:
            self.command("char", code=ord(character))


def wait_for_api(timeout: float = 45.0) -> ControlClient:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            client = ControlClient()
            client.command("ping")
            return client
        except (OSError, ConnectionError, RuntimeError, json.JSONDecodeError):
            time.sleep(0.25)
    raise TimeoutError("reloop_control API was not reachable on 127.0.0.1:18737")


def sample(client: ControlClient, label: str, output: list[dict[str, Any]]) -> dict[str, Any]:
    state = client.command("state")
    state["label"] = label
    state["host_time"] = time.time()
    output.append(state)
    print(json.dumps(state, sort_keys=True))
    return state


def run_scenario(output_path: Path, settle: float) -> None:
    states: list[dict[str, Any]] = []
    client = wait_for_api()
    try:
        client.command("focus")
        # RUN_START can precede the final class-selection handoff by a few
        # frames in the replacement. Wait until normal gameplay owns input.
        time.sleep(max(3.0, settle))
        baseline = sample(client, "baseline", states)

        # Chat: opening T must own the mouse and stop GTA control/camera input.
        client.command("char", code=ord("t"))
        time.sleep(settle)
        sample(client, "chat_open", states)
        client.key(ord("W"), "down")
        client.command("mouse", action="move", x=max(10, baseline["client_w"] // 2 + 80),
                       y=max(10, baseline["client_h"] // 2))
        time.sleep(settle)
        sample(client, "chat_input_attempt", states)
        client.key(ord("W"), "up")
        client.text("reloop chat interaction")
        client.command("window_key", vk=13)
        time.sleep(settle)
        sample(client, "chat_closed", states)

        # Scoreboard: TAB owns mouse/input; first player row is centered below header.
        client.key(9, "down")
        time.sleep(settle)
        scoreboard = sample(client, "scoreboard_open", states)
        row_x = scoreboard["client_w"] // 2
        row_y = scoreboard["client_h"] // 2
        # OBSERVED_037 calibration: R5 accepts RMB as the explicit scoreboard
        # cursor trigger. The replacement already enables mouse mode on TAB;
        # this event is harmless there and lets us compare the interactive path.
        client.command("mouse", action="right_click", x=row_x, y=row_y)
        time.sleep(settle)
        sample(client, "scoreboard_mouse_mode", states)
        client.key(ord("W"), "down")
        client.command("mouse", action="move", x=row_x + 90, y=row_y)
        time.sleep(settle)
        sample(client, "scoreboard_input_attempt", states)
        client.key(ord("W"), "up")
        # Scan the vertically centred scoreboard area. Original 0.3.7 and the
        # replacement use different test-prefix video modes, so absolute pixel
        # coordinates are intentionally derived from each client rectangle.
        for candidate_y in range(max(20, scoreboard["client_h"] // 5),
                                 max(21, scoreboard["client_h"] * 4 // 5), 12):
            client.command("mouse", action="double_click", x=row_x, y=candidate_y)
            time.sleep(0.15)
        time.sleep(0.5)
        time.sleep(settle)
        sample(client, "scoreboard_clicked", states)
        client.key(9, "up")
        time.sleep(settle)
        sample(client, "scoreboard_closed", states)
    finally:
        client.close()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(states, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["ping", "state", "scenario"])
    parser.add_argument("--output", type=Path, default=Path("ui-interaction-states.json"))
    parser.add_argument("--settle", type=float, default=0.8)
    args = parser.parse_args()
    if args.command == "scenario":
        run_scenario(args.output, args.settle)
    else:
        client = wait_for_api()
        try:
            print(json.dumps(client.command(args.command), indent=2, sort_keys=True))
        finally:
            client.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
