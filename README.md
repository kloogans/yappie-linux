# yappie-linux

Voice dictation for Linux. Press a key, talk, press again, and the transcribed text gets pasted into whatever window you're focused on. Works on any Wayland compositor.

Runs as a lightweight daemon that keeps your whisper model loaded in memory, so transcription starts the moment you stop talking. Supports on-device transcription via [whisper.cpp](https://github.com/ggml-org/whisper.cpp) with GPU acceleration, cloud APIs, or both with automatic fallback.

Part of the [yappie](https://github.com/kloogans) family.

## How it works

1. The `yappied` daemon starts and loads your whisper model onto the GPU
2. Press your dictation key. Yappie captures audio from PipeWire directly into memory
3. Press it again. Audio goes straight to whisper.cpp for transcription (no temp files, no disk I/O)
4. Transcribed text is copied to your clipboard and pasted into the focused window

The daemon stays responsive while transcription runs in the background. You can check status, swap models, or start a new recording the moment the previous one finishes.

## Install

### Arch Linux (AUR)

```bash
yay -S yappie    # or paru, pikaur, etc.
```

For on-device transcription, also install whisper.cpp:

```bash
yay -S whisper.cpp           # CPU + Vulkan
# or: yay -S whisper.cpp-cuda   # NVIDIA CUDA
```

### Any distro (from source)

```bash
git clone https://github.com/kloogans/yappie-linux.git
cd yappie-linux
bash install.sh
```

Requires: `meson`, `ninja`, `gcc`, `libpipewire`, `libcurl`. Optional: `whisper.cpp` for local transcription.

## Quick start

```bash
# Set up config
mkdir -p ~/.config/yappie
cp /usr/share/yappie/config.example.toml ~/.config/yappie/config.toml
$EDITOR ~/.config/yappie/config.toml

# Download a model (if using local transcription)
yappie model download base.en

# Start the daemon
systemctl --user enable --now yappied

# Bind to a key and go
yappie toggle
```

## CLI

```
yappie toggle                  Start/stop recording
yappie status                  Show daemon state
yappie config                  Show loaded backends

yappie model list              Available models and download status
yappie model download <name>   Download a model from HuggingFace
yappie model use <name>        Hot-swap the active model (no restart)

yappie shutdown                Stop the daemon
```

## Models

Models are downloaded from HuggingFace and stored in `~/.local/share/yappie/models/`. The `.en` variants are English-only and faster.

| Model | Size | RAM | Notes |
|---|---|---|---|
| `tiny.en` | 75 MB | ~1 GB | Fastest, lowest accuracy |
| `base.en` | 142 MB | ~1 GB | Good balance for short dictation |
| `small.en` | 466 MB | ~2 GB | Better accuracy |
| `medium.en` | 1.5 GB | ~5 GB | High accuracy |
| `large-v3-turbo` | 1.6 GB | ~6 GB | Best speed/accuracy ratio |
| `large-v3` | 2.9 GB | ~10 GB | Maximum accuracy |

Drop the `.en` suffix for multilingual models with auto language detection.

Swap models on the fly without restarting the daemon:

```bash
yappie model use small.en
```

## Backends

Yappie supports three backend types. Configure one or more in `~/.config/yappie/config.toml`. Backends are tried in order, and the first success wins.

### Local whisper.cpp (on-device)

No network, no API keys. Audio goes straight from your microphone to the model. Supports CPU and GPU acceleration (CUDA, Vulkan).

```toml
[[backend]]
name = "local"
type = "local"
model = "base.en"
# language = "en"    # omit for auto-detect
# gpu = true         # default
```

### OpenAI-compatible API

Works with any service that implements `/v1/audio/transcriptions`:

| Service | Base URL | API key |
|---|---|---|
| [OpenAI](https://platform.openai.com) | `https://api.openai.com/v1` | Required |
| [Groq](https://groq.com) | `https://api.groq.com/openai/v1` | Required |
| [Speaches](https://github.com/speaches-ai/speaches) | `http://localhost:8000/v1` | No |
| [LocalAI](https://localai.io) | `http://localhost:8080/v1` | No |

```toml
[[backend]]
name = "groq"
type = "api"
url = "https://api.groq.com/openai/v1"
model = "whisper-large-v3-turbo"
api_key = "your-api-key-here"
```

API keys can be set inline or stored in a file:

```toml
api_key_file = "~/.config/yappie/keys/groq"
```

### Custom TCP

For custom transcription servers that accept raw WAV audio over a TCP socket and return UTF-8 text.

```toml
[[backend]]
name = "my-server"
type = "tcp"
host = "127.0.0.1"
port = 9876
```

## Configuration

`~/.config/yappie/config.toml`

Backends are tried in the order they appear. You can mix all three types freely. A common setup is local whisper as the primary with a cloud API as fallback:

```toml
[[backend]]
name = "local"
type = "local"
model = "base.en"

[[backend]]
name = "groq"
type = "api"
url = "https://api.groq.com/openai/v1"
model = "whisper-large-v3-turbo"
api_key_file = "~/.config/yappie/keys/groq"
```

## Keybinding

Bind `yappie toggle` to a key in your compositor.

Hyprland:

```
bindd = SUPER, D, Dictation, exec, yappie toggle
```

Sway:

```
bindsym $mod+d exec yappie toggle
```

Any other Wayland compositor works too. Yappie detects Hyprland and Sway for smart terminal paste (Ctrl+Shift+V), and falls back to Ctrl+V everywhere else.

## Dependencies

Installed automatically via the AUR package or `install.sh`:

| Package | Used for |
|---|---|
| PipeWire | Audio capture |
| libcurl | API backends and model downloads |
| ydotool | Simulated keypresses for pasting |
| wl-clipboard | Clipboard access |
| libnotify | Desktop notifications |

**Optional:**

| Package | Used for |
|---|---|
| whisper.cpp | On-device transcription |
| Hyprland / Sway | Smart terminal paste detection |

## Architecture

Yappie is two binaries built from one C project:

- **`yappied`** is a persistent daemon that keeps your whisper model loaded in GPU memory, captures audio via PipeWire, and handles transcription. It uses PipeWire's event loop for IPC and audio, with a worker thread for blocking operations (inference, network, paste).
- **`yappie`** is a thin CLI that sends commands to the daemon over a Unix socket.

Audio is captured as 16kHz mono float samples and passed directly to whisper.cpp with no intermediate WAV encoding or disk I/O. WAV encoding only happens for remote API and TCP backends, and even then it's done in memory.

## Building

```bash
meson setup build --buildtype=release
meson compile -C build
```

Build options:

```
-Dwhisper=enabled|disabled|auto    Local transcription (default: auto-detect)
-Dsystemd=enabled|disabled|auto    systemd notify support (default: auto-detect)
```

## Uninstall

If installed from source:

```bash
bash uninstall.sh
```

If installed from AUR, just remove the package.

## License

MIT
