#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PACKAGES_FILE="${SCRIPT_DIR}/packages.fedora.txt"
REQUIREMENTS_FILE="${SCRIPT_DIR}/requirements.txt"

if [[ ! -f "$PACKAGES_FILE" ]]; then
  echo "Missing package list: $PACKAGES_FILE" >&2
  exit 2
fi

sudo_cmd=()
if [[ "$(id -u)" -ne 0 ]]; then
  if command -v sudo >/dev/null 2>&1; then
    sudo_cmd=(sudo)
  else
    echo "Need root or sudo inside the toolbox to install packages." >&2
    exit 3
  fi
fi

if command -v dnf >/dev/null 2>&1; then
  mapfile -t packages <"$PACKAGES_FILE"
  if [[ "${#packages[@]}" -gt 0 ]]; then
    "${sudo_cmd[@]}" dnf install -y "${packages[@]}"
  fi
else
  echo "Unsupported package manager inside toolbox; provision.sh currently expects dnf." >&2
  exit 4
fi

if [[ -f "$REQUIREMENTS_FILE" ]]; then
  python3 -m pip install --upgrade pip
  python3 -m pip install -r "$REQUIREMENTS_FILE"
fi
