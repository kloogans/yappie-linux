#!/bin/bash
# Uninstall yappie

set -euo pipefail

BIN_DIR="${HOME}/.local/bin"
VENV_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/yappie/venv"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/yappie"

echo "Uninstalling yappie..."

# Stop server if running
pkill -f yappie-server 2>/dev/null && echo "Stopped server" || true
rm -f "${XDG_RUNTIME_DIR:-/tmp}/yappie.sock"

# Remove scripts
rm -f "$BIN_DIR/yappie" "$BIN_DIR/yappie-server"

# Remove venv
if [ -d "$VENV_DIR" ]; then
    rm -rf "$VENV_DIR"
    rmdir --ignore-fail-on-non-empty "$(dirname "$VENV_DIR")" 2>/dev/null || true
    echo "Removed venv"
fi

echo ""
echo "Done. Config left at $CONFIG_DIR/config (delete manually if wanted)."
echo "Remove the keybinding from your Hyprland config."
