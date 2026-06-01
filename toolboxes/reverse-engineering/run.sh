#!/usr/bin/env bash
set -euo pipefail

CONTAINER_NAME="${RE_TOOLBOX_NAME:-reverse-engineering}"

if ! command -v toolbox >/dev/null 2>&1; then
  echo "Missing command: toolbox" >&2
  exit 2
fi

if [[ $# -eq 0 ]]; then
  exec toolbox enter -c "$CONTAINER_NAME"
fi

exec toolbox run -c "$CONTAINER_NAME" "$@"
