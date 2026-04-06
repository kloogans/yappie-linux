#!/bin/bash
# Install yappie from source using Meson

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/yappie"
SERVICE_DIR="${HOME}/.config/systemd/user"

echo "Building yappie..."

# Check build dependencies
for cmd in meson ninja gcc pkg-config; do
    command -v "$cmd" >/dev/null 2>&1 || {
        echo "Missing build dependency: $cmd"
        echo "On Arch Linux: sudo pacman -S meson ninja gcc pkgconf"
        exit 1
    }
done

# Check runtime dependencies
MISSING=()
for cmd in ydotool wl-copy notify-send; do
    command -v "$cmd" >/dev/null 2>&1 || MISSING+=("$cmd")
done

if [ ${#MISSING[@]} -gt 0 ]; then
    echo "Missing runtime dependencies: ${MISSING[*]}"
    echo ""
    echo "On Arch Linux:"
    echo "  sudo pacman -S ydotool wl-clipboard libnotify"
    echo ""
    echo "Install missing packages and re-run this script."
    exit 1
fi

# Build
cd "$SCRIPT_DIR"
meson setup build --prefix="$HOME/.local" --buildtype=release 2>/dev/null || meson setup build --prefix="$HOME/.local" --buildtype=release --wipe
meson compile -C build
meson install -C build

# Create config directory with example config
mkdir -p "$CONFIG_DIR"
if [ ! -f "$CONFIG_DIR/config.toml" ]; then
    cp "$SCRIPT_DIR/config.example.toml" "$CONFIG_DIR/config.toml"
    echo "Created config at $CONFIG_DIR/config.toml"
else
    echo "Config already exists at $CONFIG_DIR/config.toml (not overwritten)"
fi

# Install systemd service
mkdir -p "$SERVICE_DIR"
sed "s|/usr/bin/yappied|$HOME/.local/bin/yappied|" "$SCRIPT_DIR/data/yappied.service" > "$SERVICE_DIR/yappied.service"

echo ""
echo "Done! Next steps:"
echo ""
echo "  1. Edit config:  \$EDITOR $CONFIG_DIR/config.toml"
echo "  2. (Optional) Download a model:  yappie model download base.en"
echo "  3. Start daemon:  systemctl --user enable --now yappied"
echo "  4. Bind key:  yappie toggle"
echo ""
