#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export JAVA_HOME="$SCRIPT_DIR/jdk-21.0.11+10"
export XDG_CONFIG_HOME="$SCRIPT_DIR/.config"
export XDG_CACHE_HOME="$SCRIPT_DIR/.cache"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME"
exec "$SCRIPT_DIR/ghidra_12.1.2_PUBLIC/ghidra_12.1.2_PUBLIC/support/analyzeHeadless" "$@"
