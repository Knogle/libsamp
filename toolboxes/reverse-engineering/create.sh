#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER_NAME="${RE_TOOLBOX_NAME:-reverse-engineering}"
DISTRO="${RE_TOOLBOX_DISTRO:-fedora}"
RELEASE="${RE_TOOLBOX_RELEASE:-40}"

if ! command -v toolbox >/dev/null 2>&1; then
  echo "Missing command: toolbox" >&2
  exit 2
fi

if ! toolbox list 2>/dev/null | awk '{print $1}' | grep -Fxq "$CONTAINER_NAME"; then
  toolbox create -y -c "$CONTAINER_NAME" --distro "$DISTRO" --release "$RELEASE"
fi

if toolbox run -c "$CONTAINER_NAME" bash -lc "'$SCRIPT_DIR/provision.sh'"; then
  :
elif command -v podman >/dev/null 2>&1; then
  podman exec -u 0 "$CONTAINER_NAME" bash -lc "'$SCRIPT_DIR/provision.sh'"
else
  echo "Provisioning failed via toolbox and podman is unavailable for root fallback." >&2
  exit 3
fi

echo "Toolbox ready: $CONTAINER_NAME"
echo "Enter with: toolbox enter -c $CONTAINER_NAME"
