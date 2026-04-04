# yappie-linux

Fast, local-first dictation for Linux/Wayland. Press a key, talk, press again, and the transcribed text gets pasted into whatever window you're focused on.

Part of the [yappie](https://github.com/kloogans) family. Local dictation for every platform.

Yappie is a thin client. It records audio, sends it to any transcription backend you configure, and pastes the result. Bring your own backend: a local server, a cloud API, or both with automatic fallback.

## How it works

1. Press your dictation key to start recording via PipeWire
2. Press it again to stop
3. Yappie sends the audio to your configured backends in order until one succeeds
4. The transcribed text is copied to your clipboard and pasted into the focused window

## Install

### Arch Linux (AUR)

```bash
yay -S yappie
```

Copy the example config and edit it:

```bash
mkdir -p ~/.config/yappie
cp /usr/share/yappie/config.example.toml ~/.config/yappie/config.toml
$EDITOR ~/.config/yappie/config.toml
```

### From source

```bash
git clone https://github.com/kloogans/yappie-linux.git
cd yappie-linux
bash install.sh
```

Then edit `~/.config/yappie/config.toml` to add at least one backend.

### Keybinding

Add this to your Hyprland config:

```
bindd = SUPER, D, Dictation, exec, yappie
```

## Backends

Yappie supports two backend types. Configure one or more in `~/.config/yappie/config.toml`. Backends are tried in order, and the first success wins.

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

Backends are tried in the order they appear. You can mix API and TCP backends freely.

```toml
# Local server as primary, cloud as fallback
[[backend]]
name = "local"
type = "api"
url = "http://localhost:8000/v1"
model = "deepdml/faster-whisper-large-v3-turbo-ct2"

[[backend]]
name = "groq"
type = "api"
url = "https://api.groq.com/openai/v1"
model = "whisper-large-v3-turbo"
api_key = "gsk_..."
```

## Dependencies

| Package | Used for |
|---|---|
| PipeWire | Audio recording (`pw-record`) |
| curl | API backend requests |
| nmap | TCP backend communication (`ncat`) |
| jq | JSON parsing |
| ydotool | Simulated keypresses for pasting |
| wl-clipboard | Clipboard access (`wl-copy`) |
| libnotify | Desktop notifications |
| Hyprland | Window class detection (`hyprctl`) |

On Arch:

```bash
sudo pacman -S pipewire curl nmap jq ydotool wl-clipboard libnotify hyprland
```

## Uninstall

```bash
bash uninstall.sh
```

## License

MIT
