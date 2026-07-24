#!/usr/bin/env python3
"""Deterministic original-vs-replacement SA-MP compatibility runner."""

from __future__ import annotations

import argparse
import contextlib
import dataclasses
import datetime as dt
import hashlib
import json
import os
import re
import shlex
import shutil
import signal
import socket
import sqlite3
import subprocess
import sys
import time
import tomllib
from pathlib import Path
from typing import Any, Iterable, TextIO


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
DEFAULT_CONFIG = SCRIPT_DIR / "reloop.toml"
GTA_RELATIVE_ROOT = Path("drive_c/Program Files (x86)/Rockstar Games/GTA San Andreas")
CLIENT_LOG_NAMES = (
    "samp_runtime.log",
    "samp_net_trace.log",
    "samp_hook_trace.log",
    "samp_probe.log",
    "reloop_control.log",
    "samp_re.log",
)
CONTROL_ASI = REPO_ROOT / "build-reloop-control/reloop_control.asi"
CONTROL_CLIENT = SCRIPT_DIR / "control_client.py"
PASS_VERDICTS = {"PASS", "PASS_WITH_WARNINGS"}
CRASH_VERDICTS = {"PRECONNECT_CRASH", "RUNTIME_CRASH"}
SERVER_READY_PATTERN = re.compile(r"Legacy Network started on port\s+(\d+)")
RESULT_PATTERN = re.compile(r"\[test_cmds\]\s+(.*)")
KEY_VALUE_PATTERN = re.compile(r"([a-z_]+)=([^\s]+)")


class ReLoopError(RuntimeError):
    """Expected operator-facing runner failure."""


@dataclasses.dataclass(frozen=True)
class ClientProfile:
    name: str
    prefix: Path
    lutris_game_id: int | None
    lutris_config: Path | None

    @property
    def gta_root(self) -> Path:
        return self.prefix / GTA_RELATIVE_ROOT

    @property
    def samp_exe(self) -> Path:
        return self.gta_root / "samp.exe"

    @property
    def gta_exe(self) -> Path:
        return self.gta_root / "gta_sa.exe"

    @property
    def samp_dll(self) -> Path:
        return self.gta_root / "samp.dll"


@dataclasses.dataclass(frozen=True)
class Settings:
    config_path: Path
    server_root: Path
    server_executable: Path
    server_config: Path
    pawn_source: Path
    pawn_output: Path
    pawn_compiler: Path
    build_script: Path
    built_dll: Path
    artifacts_root: Path
    proton_root: Path
    device_helper_source: Path
    device_helper_filename: str
    device_helper_sha256: str
    host: str
    port: int
    nickname: str
    group: str
    delay_ms: int
    server_ready_timeout_s: int
    run_timeout_s: int
    shutdown_timeout_s: int
    crash_attempts: int
    autospawn: bool
    clients: dict[str, ClientProfile]


@dataclasses.dataclass(frozen=True)
class FileSnapshot:
    path: Path
    size: int
    inode: int | None

    @classmethod
    def take(cls, path: Path) -> "FileSnapshot":
        try:
            stat = path.stat()
        except FileNotFoundError:
            return cls(path, 0, None)
        return cls(path, stat.st_size, stat.st_ino)

    def capture_append(self, destination: Path) -> int:
        if not self.path.is_file():
            return 0
        current = self.path.stat()
        offset = self.size if current.st_ino == self.inode and current.st_size >= self.size else 0
        destination.parent.mkdir(parents=True, exist_ok=True)
        copied = 0
        with self.path.open("rb") as source, destination.open("wb") as target:
            source.seek(offset)
            while chunk := source.read(1024 * 1024):
                target.write(chunk)
                copied += len(chunk)
        return copied


@dataclasses.dataclass
class ManagedProcess:
    process: subprocess.Popen[bytes]
    log_handle: TextIO
    name: str

    def stop(self, timeout: int) -> None:
        if self.process.poll() is not None:
            self.log_handle.close()
            return
        with contextlib.suppress(ProcessLookupError):
            os.killpg(self.process.pid, signal.SIGTERM)
        try:
            self.process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            with contextlib.suppress(ProcessLookupError):
                os.killpg(self.process.pid, signal.SIGKILL)
            with contextlib.suppress(subprocess.TimeoutExpired):
                self.process.wait(timeout=5)
        self.log_handle.close()


def _resolve_repo_path(value: str) -> Path:
    path = Path(value).expanduser()
    return path if path.is_absolute() else REPO_ROOT / path


def load_settings(path: Path) -> Settings:
    with path.open("rb") as handle:
        raw = tomllib.load(handle)
    workspace = raw["workspace"]
    run = raw["run"]
    runner = raw["runner"]
    device_selection = raw["device_selection"]
    clients = {
        name: ClientProfile(
            name=name,
            prefix=Path(data["prefix"]).expanduser(),
            lutris_game_id=int(data["lutris_game_id"]) if "lutris_game_id" in data else None,
            lutris_config=Path(data["lutris_config"]).expanduser() if "lutris_config" in data else None,
        )
        for name, data in raw["client"].items()
    }
    return Settings(
        config_path=path,
        server_root=_resolve_repo_path(workspace["server_root"]),
        server_executable=_resolve_repo_path(workspace["server_executable"]),
        server_config=_resolve_repo_path(workspace["server_config"]),
        pawn_source=_resolve_repo_path(workspace["pawn_source"]),
        pawn_output=_resolve_repo_path(workspace["pawn_output"]),
        pawn_compiler=_resolve_repo_path(workspace["pawn_compiler"]),
        build_script=_resolve_repo_path(workspace["build_script"]),
        built_dll=_resolve_repo_path(workspace["built_dll"]),
        artifacts_root=_resolve_repo_path(workspace["artifacts_root"]),
        proton_root=Path(runner["proton_root"]).expanduser(),
        device_helper_source=Path(device_selection["source"]).expanduser(),
        device_helper_filename=str(device_selection["filename"]),
        device_helper_sha256=str(device_selection["sha256"]),
        host=str(run["host"]),
        port=int(run["port"]),
        nickname=str(run["nickname"]),
        group=str(run["group"]),
        delay_ms=int(run["delay_ms"]),
        server_ready_timeout_s=int(run["server_ready_timeout_s"]),
        run_timeout_s=int(run["run_timeout_s"]),
        shutdown_timeout_s=int(run["shutdown_timeout_s"]),
        crash_attempts=max(3, int(run.get("crash_attempts", 3))),
        autospawn=bool(run["autospawn"]),
        clients=clients,
    )


def sha256(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def utc_timestamp() -> str:
    return dt.datetime.now(dt.UTC).isoformat(timespec="seconds")


def run_id(client: str, group: str) -> str:
    timestamp = dt.datetime.now().astimezone().strftime("%Y%m%d-%H%M%S")
    return f"{timestamp}-{client}-{group}-{os.getpid()}"


def request_id() -> int:
    # Pawn cells are signed 32-bit. Keep the value positive and sufficiently unique.
    return ((int(time.time() * 1000) ^ os.getpid()) % 2_000_000_000) + 1


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def append_event(path: Path, state: str, **details: Any) -> None:
    payload = {"time": utc_timestamp(), "state": state, **details}
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(payload, sort_keys=True) + "\n")
    print(f"[{state}]", json.dumps(details, sort_keys=True) if details else "")


def process_info() -> Iterable[tuple[int, Path | None, list[str], bytes]]:
    proc = Path("/proc")
    for entry in proc.iterdir():
        if not entry.name.isdigit():
            continue
        pid = int(entry.name)
        try:
            exe = entry.joinpath("exe").resolve()
        except (FileNotFoundError, PermissionError, OSError):
            exe = None
        try:
            cmdline = entry.joinpath("cmdline").read_bytes().split(b"\0")
            args = [item.decode("utf-8", "replace") for item in cmdline if item]
        except (FileNotFoundError, PermissionError, OSError):
            args = []
        try:
            environment = entry.joinpath("environ").read_bytes()
        except (FileNotFoundError, PermissionError, OSError):
            environment = b""
        yield pid, exe, args, environment


def server_pids(executable: Path) -> set[int]:
    expected = executable.resolve()
    found: set[int] = set()
    for pid, exe, args, _environment in process_info():
        if exe == expected:
            found.add(pid)
        elif args and Path(args[0]).name == expected.name and str(expected) in args:
            found.add(pid)
    return found


def prefix_pids(prefix: Path) -> set[int]:
    needle = str(prefix.resolve()).encode()
    return {
        pid
        for pid, _exe, _args, environment in process_info()
        if needle in environment
    }


def terminate_pids(pids: set[int], timeout: int) -> set[int]:
    def alive(pid: int) -> bool:
        try:
            fields = Path(f"/proc/{pid}/stat").read_text(encoding="ascii").split()
        except (FileNotFoundError, PermissionError, OSError):
            return False
        return len(fields) > 2 and fields[2] != "Z"

    own_pid = os.getpid()
    targets = {pid for pid in pids if pid > 1 and pid != own_pid}
    for pid in targets:
        with contextlib.suppress(ProcessLookupError, PermissionError):
            os.kill(pid, signal.SIGTERM)
    deadline = time.monotonic() + timeout
    while targets and time.monotonic() < deadline:
        targets = {pid for pid in targets if alive(pid)}
        time.sleep(0.2)
    for pid in targets:
        with contextlib.suppress(ProcessLookupError, PermissionError):
            os.kill(pid, signal.SIGKILL)
    return {pid for pid in targets if alive(pid)}


def udp_port_available(host: str, port: int) -> bool | None:
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    except PermissionError:
        return None
    try:
        sock.bind((host, port))
    except OSError:
        return False
    finally:
        sock.close()
    return True


def lutris_db_row(game_id: int | None) -> dict[str, Any] | None:
    if game_id is None:
        return None
    database = Path.home() / ".local/share/lutris/pga.db"
    if not database.is_file():
        return None
    try:
        connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        row = connection.execute("SELECT * FROM games WHERE id = ?", (game_id,)).fetchone()
        connection.close()
    except sqlite3.Error:
        return None
    return dict(row) if row else None


def doctor(settings: Settings, json_output: bool = False) -> bool:
    checks: list[dict[str, Any]] = []

    def add(name: str, ok: bool, detail: str, required: bool = True) -> None:
        checks.append({"name": name, "ok": ok, "required": required, "detail": detail})

    for command in ("python3", "sha256sum"):
        found = shutil.which(command)
        add(f"command:{command}", found is not None, found or "not found")
    for name, path, executable in (
        ("server", settings.server_executable, True),
        ("server_config", settings.server_config, False),
        ("pawn_compiler", settings.pawn_compiler, True),
        ("pawn_source", settings.pawn_source, False),
        ("build_script", settings.build_script, True),
    ):
        ok = path.is_file() and (not executable or os.access(path, os.X_OK))
        add(name, ok, str(path))
    add("display", bool(os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")),
        f"DISPLAY={os.environ.get('DISPLAY', '')} WAYLAND_DISPLAY={os.environ.get('WAYLAND_DISPLAY', '')}")
    wine_runner = settings.proton_root / "files/bin/wine"
    add("proton_runner", wine_runner.is_file() and os.access(wine_runner, os.X_OK), str(wine_runner))
    helper_hash = sha256(settings.device_helper_source)
    add("device_helper_source", helper_hash == settings.device_helper_sha256,
        f"{settings.device_helper_source} sha256={helper_hash}")

    for name, profile in settings.clients.items():
        add(f"{name}:prefix", profile.prefix.is_dir(), str(profile.prefix))
        add(f"{name}:gta_root", profile.gta_root.is_dir(), str(profile.gta_root))
        add(f"{name}:samp.exe", profile.samp_exe.is_file(), str(profile.samp_exe))
        add(f"{name}:samp.dll", profile.samp_dll.is_file(),
            f"{profile.samp_dll} sha256={sha256(profile.samp_dll)}")
        config_ok = profile.lutris_config is not None and profile.lutris_config.is_file()
        config_text = profile.lutris_config.read_text(encoding="utf-8", errors="replace") if config_ok else ""
        config_ok = config_ok and str(profile.prefix) in config_text
        add(f"{name}:lutris_config", config_ok, str(profile.lutris_config), required=False)
        db_row = lutris_db_row(profile.lutris_game_id)
        add(f"{name}:lutris_game_id", db_row is not None,
            f"id={profile.lutris_game_id} row={db_row}", required=False)
        installed_helper = profile.gta_root / settings.device_helper_filename
        installed_hash = sha256(installed_helper)
        add(f"{name}:device_helper", installed_hash == settings.device_helper_sha256,
            f"{installed_helper} sha256={installed_hash}", required=False)
        active = prefix_pids(profile.prefix)
        add(f"{name}:inactive", not active, f"active_pids={sorted(active)}", required=False)

    active_server = server_pids(settings.server_executable)
    add("server_inactive", not active_server, f"active_pids={sorted(active_server)}", required=False)
    port_available = udp_port_available(settings.host, settings.port)
    add("udp_port_available", port_available is True,
        f"{settings.host}:{settings.port} available={port_available}", required=False)
    ok = all(check["ok"] for check in checks if check["required"])
    if json_output:
        print(json.dumps({"ok": ok, "checks": checks}, indent=2, sort_keys=True))
    else:
        for check in checks:
            label = "PASS" if check["ok"] else ("WARN" if not check["required"] else "FAIL")
            print(f"{label:4} {check['name']}: {check['detail']}")
        print(f"doctor: {'PASS' if ok else 'FAIL'}")
    return ok


def run_logged(command: list[str], cwd: Path, log_path: Path, label: str) -> None:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"[{label}] {shlex.join(command)}")
    with log_path.open("wb") as log:
        result = subprocess.run(command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        raise ReLoopError(f"{label} failed with exit code {result.returncode}; see {log_path}")


def build_pawn(settings: Settings, log_path: Path) -> None:
    command = [
        str(settings.pawn_compiler),
        settings.pawn_source.name,
        f"-D{settings.pawn_source.parent.relative_to(settings.server_root)}",
        "-i../qawno/include",
        f"-o{settings.pawn_output.name}",
    ]
    run_logged(command, settings.server_root, log_path, "PAWN_BUILD")
    if not settings.pawn_output.is_file():
        raise ReLoopError(f"Pawn compiler returned success but did not create {settings.pawn_output}")


def build_all(settings: Settings, artifact_dir: Path, dll: bool = True, pawn: bool = True) -> None:
    artifact_dir.mkdir(parents=True, exist_ok=True)
    if pawn:
        build_pawn(settings, artifact_dir / "pawn-build.log")
    if dll:
        run_logged([str(settings.build_script)], REPO_ROOT, artifact_dir / "dll-build.log", "DLL_BUILD")
        if not settings.built_dll.is_file():
            raise ReLoopError(f"build did not create {settings.built_dll}")
    write_json(artifact_dir / "build.json", {
        "time": utc_timestamp(),
        "pawn_output": str(settings.pawn_output),
        "pawn_sha256": sha256(settings.pawn_output),
        "built_dll": str(settings.built_dll),
        "built_dll_sha256": sha256(settings.built_dll),
    })


def deploy_replacement(settings: Settings, artifact_dir: Path) -> dict[str, Any]:
    profile = settings.clients["replacement"]
    source = settings.built_dll
    destination = profile.samp_dll
    if not source.is_file():
        raise ReLoopError(f"replacement DLL does not exist: {source}")
    if not destination.parent.is_dir() or profile.prefix not in destination.parents:
        raise ReLoopError(f"refusing deployment outside configured replacement prefix: {destination}")
    before = sha256(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    source_hash = sha256(source)
    installed_hash = sha256(destination)
    if source_hash != installed_hash:
        raise ReLoopError("deployed DLL hash does not match built DLL")
    record = {
        "time": utc_timestamp(),
        "source": str(source),
        "destination": str(destination),
        "previous_sha256": before,
        "built_sha256": source_hash,
        "installed_sha256": installed_hash,
    }
    write_json(artifact_dir / "deploy.json", record)
    print(f"[DEPLOY] {destination} sha256={installed_hash}")
    return record


def start_process(
    command: list[str], cwd: Path, log_path: Path, name: str, env: dict[str, str] | None = None
) -> ManagedProcess:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_handle = log_path.open("w", encoding="utf-8", buffering=1)
    process = subprocess.Popen(
        command,
        cwd=cwd,
        stdout=log_handle,
        stderr=subprocess.STDOUT,
        start_new_session=True,
        text=False,
        env=env,
    )
    return ManagedProcess(process=process, log_handle=log_handle, name=name)


def wait_for_text(path: Path, pattern: re.Pattern[str], timeout: int, process: subprocess.Popen[bytes] | None = None) -> re.Match[str] | None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if path.is_file():
            match = pattern.search(path.read_text(encoding="utf-8", errors="replace"))
            if match:
                return match
        if process is not None and process.poll() is not None:
            return None
        time.sleep(0.25)
    return None


def replace_existing_server(settings: Settings, mode: str) -> bool:
    pids = server_pids(settings.server_executable)
    port_available = udp_port_available(settings.host, settings.port)
    if port_available is None:
        raise ReLoopError("cannot inspect the UDP port in this environment")
    port_busy = not port_available
    if mode == "reuse":
        if not pids and not port_busy:
            raise ReLoopError("--server-mode reuse requested, but no bare server is running")
        return True
    if pids:
        if mode == "fail":
            raise ReLoopError(f"bare server is already active: pids={sorted(pids)}")
        survivors = terminate_pids(pids, settings.shutdown_timeout_s)
        if survivors:
            raise ReLoopError(f"could not stop existing bare server pids={sorted(survivors)}")
    elif port_busy:
        raise ReLoopError(f"UDP port {settings.host}:{settings.port} is occupied by an unrecognized process")
    return False


def replace_existing_client(profile: ClientProfile, mode: str, timeout: int) -> None:
    pids = prefix_pids(profile.prefix)
    if not pids:
        return
    if mode == "fail":
        raise ReLoopError(
            f"configured {profile.name} prefix is already active: pids={sorted(pids)}"
        )
    survivors = terminate_pids(pids, timeout)
    if survivors:
        raise ReLoopError(
            f"could not stop existing processes in {profile.name} prefix: pids={sorted(survivors)}"
        )


def install_device_helper(settings: Settings, profile: ClientProfile, artifact_dir: Path) -> dict[str, Any]:
    artifact_dir.mkdir(parents=True, exist_ok=True)
    source_hash = sha256(settings.device_helper_source)
    if source_hash != settings.device_helper_sha256:
        raise ReLoopError(
            f"SkipDeviceSelection source hash mismatch: expected={settings.device_helper_sha256} actual={source_hash}"
        )
    destination = profile.gta_root / settings.device_helper_filename
    previous_hash = sha256(destination)
    shutil.copy2(settings.device_helper_source, destination)
    installed_hash = sha256(destination)
    if installed_hash != settings.device_helper_sha256:
        raise ReLoopError(f"installed SkipDeviceSelection hash mismatch: {destination}")
    record = {
        "source": str(settings.device_helper_source),
        "destination": str(destination),
        "previous_sha256": previous_hash,
        "installed_sha256": installed_hash,
        "license": "MIT; upstream package ReadMe.txt and License.txt inspected",
    }
    write_json(artifact_dir / f"device-helper-{profile.name}.json", record)
    print(f"[DEVICE_HELPER] {destination} sha256={installed_hash}")
    return record


def install_control_helper(profile: ClientProfile, artifact_dir: Path) -> dict[str, Any]:
    if not CONTROL_ASI.is_file():
        raise ReLoopError(f"control ASI is not built: {CONTROL_ASI}")
    destination = profile.gta_root / CONTROL_ASI.name
    previous_hash = sha256(destination)
    shutil.copy2(CONTROL_ASI, destination)
    record = {
        "source": str(CONTROL_ASI),
        "destination": str(destination),
        "previous_sha256": previous_hash,
        "installed_sha256": sha256(destination),
        "binding": "127.0.0.1:18737",
        "protocol": "newline-json-v1",
    }
    write_json(artifact_dir / f"control-helper-{profile.name}.json", record)
    print(f"[CONTROL_HELPER] {destination} sha256={record['installed_sha256']}")
    return record


def direct_client_launch(
    settings: Settings, profile: ClientProfile, artifact_dir: Path
) -> tuple[list[str], dict[str, str]]:
    wine = settings.proton_root / "files/bin/wine"
    wine_lib = settings.proton_root / "files/lib"
    if not wine.is_file() or not os.access(wine, os.X_OK):
        raise ReLoopError(f"GE-Proton Wine runner is unavailable: {wine}")
    env = os.environ.copy()
    previous_ld_path = env.get("LD_LIBRARY_PATH", "")
    env.update({
        "LD_LIBRARY_PATH": str(wine_lib) + (f":{previous_ld_path}" if previous_ld_path else ""),
        "WINEPREFIX": str(profile.prefix),
        "WINEARCH": "win64",
        "WINEDEBUG": "-all",
        "WINEESYNC": "1",
        "WINEFSYNC": "1",
        "WINE_FULLSCREEN_FSR": "1",
        "WINE_LARGE_ADDRESS_AWARE": "1",
        "DXVK_LOG_LEVEL": "error",
        "DXVK_ENABLE_NVAPI": "1",
        "DXVK_NVAPIHACK": "0",
        "WINEDLLOVERRIDES": "d3dx9_25=n;winemenubuilder=",
        "TERM": "xterm",
    })
    command = [
        str(wine),
        str(profile.samp_exe),
        f"{settings.host}:{settings.port}",
        f"-n{settings.nickname}",
    ]
    write_json(artifact_dir / "direct-launch.json", {
        "command": command,
        "runner": str(settings.proton_root),
        "runner_wine_sha256": sha256(wine),
        "environment": {
            key: env[key]
            for key in (
                "LD_LIBRARY_PATH", "WINEPREFIX", "WINEARCH", "WINEDEBUG", "WINEESYNC",
                "WINEFSYNC", "WINE_FULLSCREEN_FSR", "WINE_LARGE_ADDRESS_AWARE",
                "DXVK_LOG_LEVEL", "DXVK_ENABLE_NVAPI", "DXVK_NVAPIHACK", "WINEDLLOVERRIDES",
            )
        },
    })
    return command, env


def take_screenshot(destination: Path) -> tuple[bool, str]:
    backends = (
        (["gnome-screenshot", "-f", str(destination)], "gnome-screenshot"),
        (["grim", str(destination)], "grim"),
    )
    for command, name in backends:
        if shutil.which(command[0]):
            try:
                result = subprocess.run(
                    command,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    check=False,
                    timeout=5,
                )
            except subprocess.TimeoutExpired:
                return False, f"{name}:timeout"
            return result.returncode == 0 and destination.is_file(), name
    return False, "no screenshot backend"


def parse_result_line(line: str) -> dict[str, str] | None:
    match = RESULT_PATTERN.search(line)
    if not match:
        return None
    return {key: value for key, value in KEY_VALUE_PATTERN.findall(match.group(1))}


def collect_request_lines(path: Path, offset: int, current_request: int) -> list[str]:
    if not path.is_file():
        return []
    with path.open("rb") as handle:
        handle.seek(offset if path.stat().st_size >= offset else 0)
        text = handle.read().decode("utf-8", "replace")
    needle = f"request={current_request}"
    return [line.rstrip("\r") for line in text.splitlines() if needle in line]


def classify_run(lines: list[str], client_returncode: int | None, client_logs: str, timed_out: bool) -> tuple[str, list[str]]:
    warnings: list[str] = []
    run_start = any("marker=RUN_START" in line for line in lines)
    run_done = next((line for line in lines if "marker=RUN_DONE" in line), None)
    run_abort = any("marker=RUN_ABORT" in line for line in lines)
    failures = [line for line in lines if " status=FAIL " in line and "marker=RUN_DONE" not in line]
    crash = "exception_filter" in client_logs or "unhandled page fault" in client_logs.lower()

    if run_done:
        if "status=FAIL" in run_done or failures:
            return "STATE_MISMATCH", warnings
        if crash or (client_returncode not in (None, 0)):
            warnings.append("client exited/crashed after RUN_DONE; scenario result remains valid")
        observations = sum(" status=OBSERVE " in line for line in lines)
        if observations:
            warnings.append(f"{observations} visual observations require screenshot/golden review")
        return ("PASS_WITH_WARNINGS" if warnings else "PASS"), warnings
    if client_returncode is not None or crash:
        return ("RUNTIME_CRASH" if run_start else "PRECONNECT_CRASH"), warnings
    if run_abort:
        return "PROTOCOL_MISMATCH", warnings
    if timed_out:
        return ("PROTOCOL_MISMATCH" if run_start else "INFRA_FAIL"), warnings
    return "INFRA_FAIL", warnings


def should_retry_crash(verdict: str, attempt: int, max_attempts: int) -> bool:
    return verdict in CRASH_VERDICTS and attempt < max_attempts


def write_summary(artifact_dir: Path, metadata: dict[str, Any], verdict: dict[str, Any]) -> None:
    lines = [
        f"# reloop run {metadata['run_id']}",
        "",
        f"- Verdict: **{verdict['verdict']}**",
        f"- Client: `{metadata['client']}`",
        f"- Prefix: `{metadata['prefix']}`",
        f"- Request: `{metadata['request_id']}`",
        f"- Scenario: `{metadata['group']}` at `{metadata['delay_ms']} ms`",
        f"- Started: `{metadata['started_at']}`",
        f"- Finished: `{verdict['finished_at']}`",
        f"- Server result records: `{verdict['result_line_count']}`",
        "",
        "## Warnings",
        "",
    ]
    warnings = verdict.get("warnings", [])
    lines.extend([f"- {warning}" for warning in warnings] or ["- None."])
    lines.extend([
        "",
        "## Evidence",
        "",
        "- `events.jsonl`: lifecycle state machine",
        "- `server.console.log`: isolated open.mp console",
        "- `server-results.log`: request-scoped `test_cmds` records",
        "- `client/`: bytes appended to the client logs during this run",
        "- `metadata.json` and `verdict.json`: machine-readable run record",
        "",
    ])
    (artifact_dir / "summary.md").write_text("\n".join(lines), encoding="utf-8")


def execute_run(
    settings: Settings,
    client_name: str,
    group: str,
    delay_ms: int,
    timeout_s: int,
    server_mode: str,
    client_mode: str,
    build: bool,
    deploy: bool,
    screenshots: bool,
    keep_client: bool,
    interaction: bool = False,
    companion_enabled: bool = False,
) -> tuple[Path, dict[str, Any]]:
    if client_name not in settings.clients:
        raise ReLoopError(f"unknown client profile: {client_name}")
    if group not in {"all", "vehicle", "player", "pvars", "ui", "labels"}:
        raise ReLoopError(f"unknown test group: {group}")
    if not 250 <= delay_ms <= 10_000:
        raise ReLoopError("delay must be between 250 and 10000 ms")

    profile = settings.clients[client_name]
    artifact_dir = settings.artifacts_root / run_id(client_name, group)
    artifact_dir.mkdir(parents=True, exist_ok=False)
    events = artifact_dir / "events.jsonl"
    current_request = request_id()
    metadata = {
        "run_id": artifact_dir.name,
        "request_id": current_request,
        "started_at": utc_timestamp(),
        "client": client_name,
        "prefix": str(profile.prefix),
        "gta_root": str(profile.gta_root),
        "lutris_game_id": profile.lutris_game_id,
        "lutris_config": str(profile.lutris_config) if profile.lutris_config is not None else None,
        "proton_root": str(settings.proton_root),
        "group": group,
        "delay_ms": delay_ms,
        "host": settings.host,
        "port": settings.port,
        "nickname": settings.nickname,
        "autospawn": settings.autospawn,
        "compatibility_contract": {
            "same_rpc_required": True,
            "same_gta_opcode_path_required": True,
            "state_verdict_does_not_imply_opcode_parity": True,
        },
        "server_mode": server_mode,
        "client_mode": client_mode,
        "device_helper_path": str(profile.gta_root / settings.device_helper_filename),
        "dll_sha256": sha256(profile.samp_dll),
        "built_dll_sha256": sha256(settings.built_dll),
        "pawn_sha256": sha256(settings.pawn_output),
    }
    write_json(artifact_dir / "metadata.json", metadata)
    append_event(events, "PREPARE", artifact=str(artifact_dir), request=current_request)

    replace_existing_client(profile, client_mode, settings.shutdown_timeout_s)
    if build:
        append_event(events, "BUILD")
        build_all(settings, artifact_dir / "build", dll=client_name == "replacement", pawn=True)
    if deploy and client_name == "replacement":
        append_event(events, "DEPLOY")
        deploy_replacement(settings, artifact_dir / "build")
    append_event(events, "DEVICE_HELPER_INSTALL")
    install_device_helper(settings, profile, artifact_dir / "build")
    if interaction:
        append_event(events, "CONTROL_HELPER_INSTALL")
        install_control_helper(profile, artifact_dir / "build")
    if not settings.pawn_output.is_file():
        raise ReLoopError(f"test filterscript is not compiled: {settings.pawn_output}")

    reused_server = replace_existing_server(settings, server_mode)
    request_file = settings.server_root / "scriptfiles/test_cmds_request.txt"
    result_file = settings.server_root / "scriptfiles/test_cmds_results.log"
    request_file.parent.mkdir(parents=True, exist_ok=True)
    request_file.write_text(
        f"{current_request} {group} {delay_ms} {1 if settings.autospawn else 0} {settings.nickname}\n",
        encoding="ascii",
    )
    result_snapshot = FileSnapshot.take(result_file)
    server_log_snapshot = FileSnapshot.take(settings.server_root / "log.txt")
    client_snapshots = {name: FileSnapshot.take(profile.gta_root / name) for name in CLIENT_LOG_NAMES}
    append_event(events, "REQUEST_QUEUED", file=str(request_file))

    server: ManagedProcess | None = None
    client: ManagedProcess | None = None
    companion: ManagedProcess | None = None
    companion_profile: ClientProfile | None = None
    companion_pre_pids: set[int] = set()
    pre_client_pids = prefix_pids(profile.prefix)
    client_returncode: int | None = None
    launcher_exit_reported = False
    timed_out = False
    screenshots_taken: list[str] = []
    screenshot_capture_available = screenshots
    seen_lines: set[str] = set()
    result_offset = result_snapshot.size
    interaction_done = False
    interaction_error: str | None = None
    try:
        if reused_server:
            append_event(events, "SERVER_READY", reused=True)
        else:
            append_event(events, "SERVER_START")
            server = start_process([str(settings.server_executable)], settings.server_root,
                                   artifact_dir / "server.console.log", "open.mp")
            ready = wait_for_text(artifact_dir / "server.console.log", SERVER_READY_PATTERN,
                                  settings.server_ready_timeout_s, server.process)
            if not ready:
                raise ReLoopError("open.mp server did not become ready; see server.console.log")
            append_event(events, "SERVER_READY", port=int(ready.group(1)))

        client_command, client_env = direct_client_launch(settings, profile, artifact_dir / "launcher")
        append_event(events, "CLIENT_START", command=shlex.join(client_command))
        client = start_process(client_command, profile.gta_root,
                               artifact_dir / "client-launcher.log", "SA-MP client", env=client_env)

        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            lines = collect_request_lines(result_file, result_offset, current_request)
            for line in lines:
                if line in seen_lines:
                    continue
                seen_lines.add(line)
                parsed = parse_result_line(line) or {}
                marker = parsed.get("marker")
                event = parsed.get("event")
                if marker:
                    append_event(events, marker, record=line)
                elif event:
                    append_event(events, "CASE_EVENT", event=event, status=parsed.get("status", ""))
                if screenshots and parsed.get("status") == "OBSERVE" and screenshot_capture_available:
                    time.sleep(min(0.35, delay_ms / 2000.0))
                    screenshot_path = artifact_dir / "screenshots" / f"{len(screenshots_taken):02d}-{event or 'observe'}.png"
                    screenshot_path.parent.mkdir(parents=True, exist_ok=True)
                    ok, backend = take_screenshot(screenshot_path)
                    append_event(events, "SCREENSHOT", ok=ok, backend=backend, file=str(screenshot_path))
                    if ok:
                        screenshots_taken.append(str(screenshot_path))
                    else:
                        screenshot_capture_available = False
                        append_event(events, "SCREENSHOT_DISABLED", reason=backend)
                if interaction and marker == "RUN_START" and not interaction_done:
                    interaction_done = True
                    append_event(events, "INTERACTION_START")
                    if companion_enabled:
                        companion_name = "replacement" if client_name == "original" else "original"
                        companion_profile = settings.clients[companion_name]
                        replace_existing_client(companion_profile, "replace", settings.shutdown_timeout_s)
                        companion_pre_pids = prefix_pids(companion_profile.prefix)
                        companion_command, companion_env = direct_client_launch(
                            settings, companion_profile, artifact_dir / "companion-launcher"
                        )
                        companion_command[-1] = "-nReLoopPeer"
                        companion = start_process(
                            companion_command, companion_profile.gta_root,
                            artifact_dir / "companion-launcher.log", "SA-MP companion", env=companion_env,
                        )
                        append_event(events, "COMPANION_START", client=companion_name,
                                     command=shlex.join(companion_command))
                        companion_join = wait_for_text(
                            artifact_dir / "server.console.log",
                            re.compile(r"RPC137 ServerJoin.*name=ReLoopPeer"), 35,
                        )
                        append_event(events, "COMPANION_READY", connected=companion_join is not None)
                    scenario_log = artifact_dir / "ui-interaction.console.log"
                    with scenario_log.open("wb") as handle:
                        scenario = subprocess.run(
                            [sys.executable, str(CONTROL_CLIENT), "scenario", "--output",
                             str(artifact_dir / "ui-interaction-states.json")],
                            cwd=REPO_ROOT, stdout=handle, stderr=subprocess.STDOUT,
                            timeout=45, check=False,
                        )
                    if scenario.returncode:
                        interaction_error = f"control scenario exited with {scenario.returncode}"
                        append_event(events, "INTERACTION_FAILED", returncode=scenario.returncode)
                    else:
                        append_event(events, "INTERACTION_DONE")
            if any("marker=RUN_DONE" in line or "marker=RUN_ABORT" in line for line in lines):
                break
            if client.process.poll() is not None:
                active_client_pids = prefix_pids(profile.prefix) - pre_client_pids
                if active_client_pids:
                    if not launcher_exit_reported:
                        append_event(events, "CLIENT_LAUNCHER_EXITED", returncode=client.process.returncode,
                                     active_prefix_pids=sorted(active_client_pids))
                        launcher_exit_reported = True
                else:
                    client_returncode = client.process.returncode
                    append_event(events, "CLIENT_EXITED", returncode=client_returncode)
                    break
            time.sleep(0.25)
        else:
            timed_out = True
            append_event(events, "TIMEOUT", seconds=timeout_s)

        if client_returncode is None and client is not None:
            client_returncode = client.process.poll()
    finally:
        with contextlib.suppress(FileNotFoundError):
            request_file.unlink()
        if client is not None and not keep_client:
            append_event(events, "CLIENT_STOP")
            client.stop(settings.shutdown_timeout_s)
            new_client_pids = prefix_pids(profile.prefix) - pre_client_pids
            survivors = terminate_pids(new_client_pids, settings.shutdown_timeout_s)
            if survivors:
                append_event(events, "CLIENT_STOP_WARNING", survivors=sorted(survivors))
        elif client is not None:
            client.log_handle.close()
        if companion is not None and companion_profile is not None:
            append_event(events, "COMPANION_STOP")
            companion.stop(settings.shutdown_timeout_s)
            companion_pids = prefix_pids(companion_profile.prefix) - companion_pre_pids
            companion_survivors = terminate_pids(companion_pids, settings.shutdown_timeout_s)
            if companion_survivors:
                append_event(events, "COMPANION_STOP_WARNING", survivors=sorted(companion_survivors))
        if server is not None:
            append_event(events, "SERVER_STOP")
            server.stop(settings.shutdown_timeout_s)

    append_event(events, "COLLECT")
    result_lines = collect_request_lines(result_file, result_offset, current_request)
    (artifact_dir / "server-results.log").write_text("\n".join(result_lines) + ("\n" if result_lines else ""), encoding="utf-8")
    server_log_snapshot.capture_append(artifact_dir / "server.log")
    combined_client_logs = ""
    for name, snapshot in client_snapshots.items():
        destination = artifact_dir / "client" / name
        if snapshot.capture_append(destination):
            combined_client_logs += destination.read_text(encoding="utf-8", errors="replace") + "\n"

    verdict_name, warnings = classify_run(result_lines, client_returncode, combined_client_logs, timed_out)
    if interaction and not interaction_done:
        warnings.append("interaction scenario did not start because RUN_START was not observed")
        if verdict_name == "PASS":
            verdict_name = "PASS_WITH_WARNINGS"
    if interaction_error is not None:
        warnings.append(interaction_error)
        if verdict_name == "PASS":
            verdict_name = "PASS_WITH_WARNINGS"
    if (client_name == "replacement" and "process_attach" in combined_client_logs and
            "process_detach: done" not in combined_client_logs):
        warnings.append("process_detach missing after runner stop; shutdown-path parity was not observed")
        if verdict_name == "PASS":
            verdict_name = "PASS_WITH_WARNINGS"
    verdict = {
        "verdict": verdict_name,
        "finished_at": utc_timestamp(),
        "request_id": current_request,
        "client_returncode": client_returncode,
        "timed_out": timed_out,
        "result_line_count": len(result_lines),
        "warnings": warnings,
        "screenshots": screenshots_taken,
    }
    write_json(artifact_dir / "verdict.json", verdict)
    write_summary(artifact_dir, metadata, verdict)
    append_event(events, "VERDICT", verdict=verdict_name)
    print(f"artifact: {artifact_dir}")
    print(f"verdict: {verdict_name}")
    return artifact_dir, verdict


def execute_with_crash_retries(
    settings: Settings,
    client_name: str,
    group: str,
    delay_ms: int,
    timeout_s: int,
    server_mode: str,
    client_mode: str,
    build: bool,
    deploy: bool,
    screenshots: bool,
    keep_client: bool,
    crash_attempts: int,
    interaction: bool = False,
    companion_enabled: bool = False,
) -> tuple[Path, dict[str, Any]]:
    series: list[dict[str, Any]] = []
    final_directory: Path | None = None
    final_verdict: dict[str, Any] | None = None
    max_attempts = max(3, crash_attempts)

    for attempt in range(1, max_attempts + 1):
        directory, verdict = execute_run(
            settings=settings,
            client_name=client_name,
            group=group,
            delay_ms=delay_ms,
            timeout_s=timeout_s,
            server_mode=server_mode,
            client_mode=client_mode,
            build=build and attempt == 1,
            deploy=deploy and attempt == 1,
            screenshots=screenshots,
            keep_client=keep_client,
            interaction=interaction,
            companion_enabled=companion_enabled,
        )
        series.append({"attempt": attempt, "artifact": str(directory), "verdict": verdict["verdict"]})
        final_directory = directory
        final_verdict = verdict
        if not should_retry_crash(verdict["verdict"], attempt, max_attempts):
            break
        print(f"[CRASH_RETRY] client={client_name} attempt={attempt}/{max_attempts} verdict={verdict['verdict']}")

    assert final_directory is not None and final_verdict is not None
    repeated_crash = len(series) >= 3 and len(series) == max_attempts and all(
        item["verdict"] in CRASH_VERDICTS for item in series
    )
    record = {
        "policy": "a crash is reproducible only after three consecutive identical-scenario attempts",
        "configured_attempts": max_attempts,
        "attempts": series,
        "repeated_crash": repeated_crash,
    }
    write_json(final_directory / "retry-series.json", record)
    if len(series) > 1 and not repeated_crash:
        final_verdict.setdefault("warnings", []).append(
            f"{len(series) - 1} earlier crash attempt(s) were not reproduced by the final attempt"
        )
        if final_verdict["verdict"] == "PASS":
            final_verdict["verdict"] = "PASS_WITH_WARNINGS"
        write_json(final_directory / "verdict.json", final_verdict)
        metadata = json.loads((final_directory / "metadata.json").read_text(encoding="utf-8"))
        write_summary(final_directory, metadata, final_verdict)
    return final_directory, final_verdict


def result_signature(run_dir: Path) -> list[tuple[str, str]]:
    signature: list[tuple[str, str]] = []
    path = run_dir / "server-results.log"
    if not path.is_file():
        return signature
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        parsed = parse_result_line(line)
        if not parsed or parsed.get("marker"):
            continue
        event = parsed.get("event")
        status = parsed.get("status")
        if event and status:
            signature.append((event, status))
    return signature


def compare_runs(reference: Path, candidate: Path, output: Path) -> dict[str, Any]:
    reference_verdict = json.loads((reference / "verdict.json").read_text(encoding="utf-8"))
    candidate_verdict = json.loads((candidate / "verdict.json").read_text(encoding="utf-8"))
    reference_signature = result_signature(reference)
    candidate_signature = result_signature(candidate)
    signatures_match = reference_signature == candidate_signature
    state_verdict = "PASS" if (
        reference_verdict["verdict"] in PASS_VERDICTS
        and candidate_verdict["verdict"] in PASS_VERDICTS
        and signatures_match
    ) else "STATE_MISMATCH"
    opcode_path_verdict = "TODO_VERIFY"
    result = {
        "reference": str(reference),
        "candidate": str(candidate),
        "reference_verdict": reference_verdict["verdict"],
        "candidate_verdict": candidate_verdict["verdict"],
        "reference_signature": reference_signature,
        "candidate_signature": candidate_signature,
        "signatures_match": signatures_match,
        "state_verdict": state_verdict,
        "opcode_path_verdict": opcode_path_verdict,
        "verdict": state_verdict if state_verdict != "PASS" else "OPCODE_PARITY_UNVERIFIED",
    }
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / "comparison.json", result)
    markdown = [
        "# reloop comparison",
        "",
        f"- Verdict: **{result['verdict']}**",
        f"- Reference: `{reference}` → `{reference_verdict['verdict']}`",
        f"- Candidate: `{candidate}` → `{candidate_verdict['verdict']}`",
        f"- Server event/status signatures match: `{signatures_match}`",
        f"- State verdict: `{state_verdict}`",
        f"- Original RPC/GTA-opcode path parity: `{opcode_path_verdict}`",
        f"- Reference events: `{len(reference_signature)}`",
        f"- Candidate events: `{len(candidate_signature)}`",
        "",
    ]
    (output / "summary.md").write_text("\n".join(markdown), encoding="utf-8")
    return result


def command_build(args: argparse.Namespace, settings: Settings) -> int:
    destination = settings.artifacts_root / f"{dt.datetime.now().astimezone():%Y%m%d-%H%M%S}-build"
    build_all(settings, destination, dll=not args.pawn_only, pawn=not args.dll_only)
    print(f"artifact: {destination}")
    return 0


def command_deploy(_args: argparse.Namespace, settings: Settings) -> int:
    destination = settings.artifacts_root / f"{dt.datetime.now().astimezone():%Y%m%d-%H%M%S}-deploy"
    destination.mkdir(parents=True, exist_ok=False)
    deploy_replacement(settings, destination)
    print(f"artifact: {destination}")
    return 0


def command_install_device_helper(_args: argparse.Namespace, settings: Settings) -> int:
    destination = settings.artifacts_root / f"{dt.datetime.now().astimezone():%Y%m%d-%H%M%S}-device-helper"
    destination.mkdir(parents=True, exist_ok=False)
    for profile in settings.clients.values():
        install_device_helper(settings, profile, destination)
    print(f"artifact: {destination}")
    return 0


def command_run(args: argparse.Namespace, settings: Settings) -> int:
    _directory, verdict = execute_with_crash_retries(
        settings=settings,
        client_name=args.client,
        group=args.group,
        delay_ms=args.delay,
        timeout_s=args.timeout,
        server_mode=args.server_mode,
        client_mode=args.client_mode,
        build=not args.no_build,
        deploy=not args.no_deploy,
        screenshots=args.screenshots,
        keep_client=args.keep_client,
        crash_attempts=args.crash_attempts,
        interaction=args.interaction,
        companion_enabled=args.companion,
    )
    return 0 if verdict["verdict"] in PASS_VERDICTS else 1


def command_matrix(args: argparse.Namespace, settings: Settings) -> int:
    matrix_dir = settings.artifacts_root.parent / "matrices" / f"{dt.datetime.now().astimezone():%Y%m%d-%H%M%S}-{args.group}"
    matrix_dir.mkdir(parents=True, exist_ok=False)
    if not args.no_build:
        build_all(settings, matrix_dir / "build", dll=True, pawn=True)
    if not args.no_deploy:
        deploy_replacement(settings, matrix_dir / "build")
    reference, _ = execute_with_crash_retries(
        settings, "original", args.group, args.delay, args.timeout, args.server_mode, args.client_mode,
        build=False, deploy=False, screenshots=args.screenshots, keep_client=False,
        crash_attempts=args.crash_attempts,
        interaction=args.interaction,
        companion_enabled=args.companion,
    )
    candidate, _ = execute_with_crash_retries(
        settings, "replacement", args.group, args.delay, args.timeout, args.server_mode, args.client_mode,
        build=False, deploy=False, screenshots=args.screenshots, keep_client=False,
        crash_attempts=args.crash_attempts,
        interaction=args.interaction,
        companion_enabled=args.companion,
    )
    result = compare_runs(reference, candidate, matrix_dir)
    print(f"matrix artifact: {matrix_dir}")
    print(f"matrix verdict: {result['verdict']}")
    return 0 if result["verdict"] == "PASS" else 1


def build_parser(settings: Settings) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="reloop", description=__doc__)
    parser.add_argument("--config", type=Path, default=settings.config_path, help="TOML configuration")
    subparsers = parser.add_subparsers(dest="command", required=True)

    doctor_parser = subparsers.add_parser("doctor", help="validate prefixes, Lutris IDs and build tools")
    doctor_parser.add_argument("--json", action="store_true")

    build_parser_ = subparsers.add_parser("build", help="compile test_cmds and the replacement DLL")
    exclusive = build_parser_.add_mutually_exclusive_group()
    exclusive.add_argument("--pawn-only", action="store_true")
    exclusive.add_argument("--dll-only", action="store_true")

    subparsers.add_parser("deploy", help="install build-win32/samp.dll in the replacement prefix")
    subparsers.add_parser("install-device-helper", help="install SkipDeviceSelection.asi in both prefixes")

    def add_run_arguments(target: argparse.ArgumentParser, include_client: bool) -> None:
        if include_client:
            target.add_argument("--client", choices=sorted(settings.clients), default="replacement")
        target.add_argument("--group", choices=["all", "vehicle", "player", "pvars", "ui", "labels"], default=settings.group)
        target.add_argument("--delay", type=int, default=settings.delay_ms)
        target.add_argument("--timeout", type=int, default=settings.run_timeout_s)
        target.add_argument("--server-mode", choices=["replace", "fail", "reuse"], default="replace")
        target.add_argument("--client-mode", choices=["replace", "fail"], default="replace")
        target.add_argument("--no-build", action="store_true")
        target.add_argument("--no-deploy", action="store_true")
        target.add_argument("--screenshots", action="store_true")
        target.add_argument("--crash-attempts", type=int, default=settings.crash_attempts,
                            help="retry PRECONNECT/RUNTIME crashes this many total times (minimum/default: 3)")
        target.add_argument("--interaction", action="store_true",
                            help="drive and capture the T/scoreboard input scenario through reloop_control.asi")
        target.add_argument("--companion", action="store_true",
                            help="also launch ReLoopPeer from the other prefix for scoreboard RPC23 tests")

    run_parser = subparsers.add_parser("run", help="run one unattended compatibility scenario")
    add_run_arguments(run_parser, include_client=True)
    run_parser.add_argument("--keep-client", action="store_true")

    matrix_parser = subparsers.add_parser("matrix", help="run original then replacement and compare")
    add_run_arguments(matrix_parser, include_client=False)

    compare_parser = subparsers.add_parser("compare", help="compare two existing run directories")
    compare_parser.add_argument("reference", type=Path)
    compare_parser.add_argument("candidate", type=Path)
    compare_parser.add_argument("--output", type=Path, required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    config_path = DEFAULT_CONFIG
    if "--config" in argv:
        index = argv.index("--config")
        try:
            config_path = Path(argv[index + 1]).expanduser().resolve()
        except IndexError as error:
            raise ReLoopError("--config requires a path") from error
    settings = load_settings(config_path)
    parser = build_parser(settings)
    args = parser.parse_args(argv)
    if args.config.resolve() != settings.config_path.resolve():
        settings = load_settings(args.config.resolve())

    if args.command == "doctor":
        return 0 if doctor(settings, args.json) else 2
    if args.command == "build":
        return command_build(args, settings)
    if args.command == "deploy":
        return command_deploy(args, settings)
    if args.command == "install-device-helper":
        return command_install_device_helper(args, settings)
    if args.command == "run":
        return command_run(args, settings)
    if args.command == "matrix":
        return command_matrix(args, settings)
    if args.command == "compare":
        result = compare_runs(args.reference.resolve(), args.candidate.resolve(), args.output.resolve())
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0 if result["verdict"] == "PASS" else 1
    parser.error("unknown command")
    return 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ReLoopError as error:
        print(f"reloop: ERROR: {error}", file=sys.stderr)
        raise SystemExit(2)
