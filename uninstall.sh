#!/bin/bash
# Uninstall yappie

set -euo pipefail

BIN_DIR="${HOME}/.local/bin"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/yappie"
SERVICE_DIR="${HOME}/.config/systemd/user"

echo "Uninstalling yappie..."

# Stop and disable daemon
systemctl --user disable --now yappied 2>/dev/null || true

# Remove binaries
rm -f "$BIN_DIR/yappie" "$BIN_DIR/yappied"

# Remove service file
rm -f "$SERVICE_DIR/yappied.service"

echo ""
echo "Done. Config left at $CONFIG_DIR/ (delete manually if wanted)."
echo "Models left at ~/.local/share/yappie/models/ (delete manually if wanted)."
echo "Remove the keybinding from your compositor config."
