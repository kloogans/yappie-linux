# Yappie Linux Rework — BYOB Architecture

## Summary

Rework yappie from a bundled model runner into a thin client that sends audio to any backend the user configures. Aligns with the macOS app's architecture: yappie records, sends, and pastes. The user brings their own transcription backend.

## What changes

- **Drop yappie-server and the Python venv.** No more bundled ML models. The client is just bash + curl.
- **Add OpenAI-compatible API backend.** Any service that implements `/v1/audio/transcriptions` works: OpenAI, Groq, faster-whisper-server, LocalAI, etc.
- **Keep TCP backend** for backwards compatibility and custom servers.
- **TOML config** replaces the shell-sourceable key=value format.
- **Ordered backend list with fallback** — try backends in order, fall back to the next on failure.
- **API key support** — inline in config or in a separate keys file.

## What stays the same

- Toggle dictation with a keybinding (Super+D)
- Records via PipeWire (pw-record)
- Pastes via wl-copy + ydotool
- Window class detection for terminal vs non-terminal paste
- Desktop notifications for status/errors
- Pure client — expects backends to already be running

## Config

`~/.config/yappie/config.toml`:

```toml
# Backends are tried in order. First success wins.

[[backend]]
name = "local"
type = "api"
url = "http://localhost:8000/v1"
model = "large-v3-turbo"

[[backend]]
name = "groq"
type = "api"
url = "https://api.groq.com/openai/v1"
model = "whisper-large-v3-turbo"
api_key = "gsk_abc123"
# Or: api_key_file = "~/.config/yappie/keys/groq"

[[backend]]
name = "home-server"
type = "tcp"
host = "192.168.1.50"
port = 9876
```

## Client flow

1. User presses Super+D — `yappie` starts recording with `pw-record`
2. User presses Super+D again — recording stops
3. yappie reads `config.toml`, iterates through enabled backends in order:
   - **API backend:** `curl` POST to `{url}/audio/transcriptions` with the WAV file as multipart form data, model name, and optional Bearer token
   - **TCP backend:** pipe WAV data to `ncat {host} {port}`, read back text
4. First backend that returns text wins. If all fail, notify the user.
5. Copy text to clipboard, paste into focused window (Ctrl+Shift+V for terminals, Ctrl+V otherwise)

## Dependencies

System (pacman): `pipewire`, `nmap`, `jq`, `ydotool`, `wl-clipboard`, `libnotify`, `hyprland`, `wireplumber`, `curl`

No Python. No venv. No pip.

## Files after rework

```
bin/yappie          # the client (bash) — rewritten
config.example.toml # example config
install.sh          # simplified — just copies script + config
uninstall.sh        # simplified
PKGBUILD            # AUR package
yappie.install      # pacman post-install hooks
README.md           # updated
LICENSE             # unchanged
```

Removed: `bin/yappie-server`, `bin/yappie-setup`

## TOML parsing in bash

Use a lightweight inline parser — TOML's `[[backend]]` array-of-tables maps cleanly to a loop. We only need to parse a small subset of TOML (strings, integers, arrays of tables). No external TOML parser dependency needed.
