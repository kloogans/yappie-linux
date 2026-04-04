#!/bin/bash
# Install yappie

set -euo pipefail

BIN_DIR="${HOME}/.local/bin"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/yappie"
VENV_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/yappie/venv"

BACKEND="${1:-faster-whisper}"

echo "Installing yappie (backend: $BACKEND)..."

# Check dependencies
MISSING=()
for cmd in pw-record ncat jq ydotool wl-copy notify-send hyprctl wpctl; do
    command -v "$cmd" >/dev/null 2>&1 || MISSING+=("$cmd")
done

if [ ${#MISSING[@]} -gt 0 ]; then
    echo "Missing dependencies: ${MISSING[*]}"
    echo ""
    echo "On Arch Linux:"
    echo "  sudo pacman -S pipewire nmap jq ydotool wl-clipboard libnotify hyprland wireplumber"
    echo ""
    echo "Install missing packages and re-run this script."
    exit 1
fi

# Create Python venv
if [ ! -d "$VENV_DIR" ]; then
    echo "Creating Python venv at $VENV_DIR..."
    python3 -m venv "$VENV_DIR"
fi

# Install backend dependencies
case "$BACKEND" in
    faster-whisper)
        echo "Installing faster-whisper..."
        "$VENV_DIR/bin/pip" install -q faster-whisper
        ;;
    moonshine)
        echo "Installing moonshine..."
        "$VENV_DIR/bin/pip" install -q useful-moonshine-onnx
        ;;
    parakeet)
        echo "Installing NeMo toolkit (this may take a minute)..."
        "$VENV_DIR/bin/pip" install -q "nemo_toolkit[asr]"
        ;;
    *)
        echo "Unknown backend: $BACKEND"
        echo "Available: faster-whisper, moonshine, parakeet"
        exit 1
        ;;
esac

# Install scripts
mkdir -p "$BIN_DIR"
cp bin/yappie "$BIN_DIR/"
chmod +x "$BIN_DIR/yappie"

# Install server with correct shebang for the venv
sed "1s|.*|#!${VENV_DIR}/bin/python3|" bin/yappie-server > "$BIN_DIR/yappie-server"
chmod +x "$BIN_DIR/yappie-server"

# Create config directory with example config
mkdir -p "$CONFIG_DIR"
if [ ! -f "$CONFIG_DIR/config" ]; then
    cp config.example "$CONFIG_DIR/config"
    # Set the chosen backend
    sed -i "s/^BACKEND=.*/BACKEND=$BACKEND/" "$CONFIG_DIR/config"
    echo "Created config at $CONFIG_DIR/config"
else
    echo "Config already exists at $CONFIG_DIR/config (not overwritten)"
fi

# Detect GPU
if command -v nvidia-smi >/dev/null 2>&1; then
    echo "NVIDIA GPU detected"
elif [ -d /sys/class/drm ] && ls /sys/class/drm/card*/device/vendor 2>/dev/null | xargs grep -ql 0x1002 2>/dev/null; then
    echo "AMD GPU detected. You may want to set DEVICE=cpu in your config."
else
    echo "No GPU detected, setting DEVICE=cpu"
    sed -i 's/^DEVICE=.*/DEVICE=cpu/' "$CONFIG_DIR/config"
    sed -i 's/^COMPUTE_TYPE=.*/COMPUTE_TYPE=float32/' "$CONFIG_DIR/config"
fi

echo ""
echo "Done! Add this to your Hyprland config (e.g. ~/.config/hypr/bindings.conf):"
echo ""
echo "  bindd = SUPER, D, Dictation, exec, yappie"
echo ""
echo "The model downloads automatically on first use."
echo ""
echo "To install additional backends later:"
echo "  bash install.sh moonshine"
echo "  bash install.sh parakeet"
