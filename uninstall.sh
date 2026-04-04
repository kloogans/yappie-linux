#!/bin/bash
# Uninstall yappie

set -euo pipefail

BIN_DIR="${HOME}/.local/bin"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/yappie"

echo "Uninstalling yappie..."

rm -f "$BIN_DIR/yappie"

echo ""
echo "Done. Config left at $CONFIG_DIR/ (delete manually if wanted)."
echo "Remove the keybinding from your Hyprland config."
