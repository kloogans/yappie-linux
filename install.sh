#!/bin/bash
# Install yappie

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="${HOME}/.local/bin"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/yappie"

echo "Installing yappie..."

# Check dependencies
MISSING=()
for cmd in pw-record curl ncat jq ydotool wl-copy notify-send hyprctl; do
    command -v "$cmd" >/dev/null 2>&1 || MISSING+=("$cmd")
done

if [ ${#MISSING[@]} -gt 0 ]; then
    echo "Missing dependencies: ${MISSING[*]}"
    echo ""
    echo "On Arch Linux:"
    echo "  sudo pacman -S pipewire curl nmap jq ydotool wl-clipboard libnotify hyprland"
    echo ""
    echo "Install missing packages and re-run this script."
    exit 1
fi

# Install script
mkdir -p "$BIN_DIR"
cp "$SCRIPT_DIR/bin/yappie" "$BIN_DIR/"
chmod +x "$BIN_DIR/yappie"

# Create config directory with example config
mkdir -p "$CONFIG_DIR"
if [ ! -f "$CONFIG_DIR/config.toml" ]; then
    cp "$SCRIPT_DIR/config.example.toml" "$CONFIG_DIR/config.toml"
    echo "Created config at $CONFIG_DIR/config.toml"
else
    echo "Config already exists at $CONFIG_DIR/config.toml (not overwritten)"
fi

echo ""
echo "Done! Edit $CONFIG_DIR/config.toml to configure your backends."
echo ""
echo "Then add this to your Hyprland config (e.g. ~/.config/hypr/bindings.conf):"
echo ""
echo "  bindd = SUPER, D, Dictation, exec, yappie"
echo ""
