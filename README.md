# hypr-dictate

Fast local dictation for Hyprland. Press a key, talk, press again, and the transcribed text gets pasted into whatever window you're focused on.

It runs a persistent server that keeps the model loaded between requests, so after the first transcription subsequent ones take around 150ms. Supports multiple backends out of the box:

| Backend | Package | Best for |
|---|---|---|
| [faster-whisper](https://github.com/SYSTRAN/faster-whisper) | `faster-whisper` | GPU transcription (default) |
| [Moonshine](https://github.com/usefulsensors/moonshine) | `useful-moonshine-onnx` | Lightweight CPU inference |
| [Parakeet](https://huggingface.co/nvidia/parakeet-tdt-0.6b-v2) | `nemo_toolkit[asr]` | Best accuracy-to-size ratio |

## How it works

1. **Press Super+D** to start recording via PipeWire
2. **Press Super+D again** to stop recording and send the audio to the server
3. The server transcribes it using whisper large-v3-turbo with greedy decoding and VAD filtering
4. The text gets copied to your clipboard and pasted into the focused window (Ctrl+Shift+V for terminals, Ctrl+V for everything else)

The server (`hypr-dictate-server`) is a small Python daemon that loads the whisper model once and listens on a unix socket. It auto-starts the first time you use it and stays running in the background.

## Install

```bash
git clone https://github.com/kloogans/hypr-dictate.git
cd hypr-dictate
bash install.sh
```

Then add this to your Hyprland config (e.g. `~/.config/hypr/bindings.conf`):

```
bindd = SUPER, D, Dictation, exec, hypr-dictate
```

The whisper model (~1.5 GB) downloads automatically the first time you dictate something.

## Dependencies

| Package | What it's for |
|---|---|
| PipeWire | Audio recording (`pw-record`) |
| nmap | Socket communication (`ncat`) |
| jq | JSON parsing for window detection |
| ydotool | Simulated keypresses for pasting |
| wl-clipboard | Clipboard access (`wl-copy`) |
| libnotify | Desktop notifications |
| Hyprland | Window class detection (`hyprctl`) |
| WirePlumber | Volume save/restore (`wpctl`) |
| Python 3.10+ | Server runtime |
| NVIDIA GPU | Recommended, but CPU works too |

On Arch:
```bash
sudo pacman -S pipewire nmap jq ydotool wl-clipboard libnotify hyprland wireplumber
```

## Configuration

Edit `~/.config/hypr-dictate/config`:

```bash
BACKEND=faster-whisper   # faster-whisper, moonshine, or parakeet
MODEL=large-v3-turbo     # model name (depends on backend)
DEVICE=cuda              # cuda, cpu, or auto
COMPUTE_TYPE=int8        # float16, int8, or float32
BEAM_SIZE=1              # 1 = fastest, 5 = most accurate
VAD_FILTER=true          # skip silence
```

Each backend has sensible defaults, so you really only need to set `BACKEND` and `MODEL`. The rest is optional.

### Switching backends

```bash
# Moonshine on CPU (lightweight, no GPU needed)
BACKEND=moonshine
MODEL=moonshine/base

# Parakeet (600M params, great accuracy)
BACKEND=parakeet
MODEL=nvidia/parakeet-tdt-0.6b-v2

# Whisper on CPU
BACKEND=faster-whisper
DEVICE=cpu
COMPUTE_TYPE=float32
MODEL=small
```

Restart the server after changing backends (`pkill hypr-dictate-server`). It will auto-start on the next dictation.

## Why it's fast

Most of the work here was finding and eliminating the things that made dictation feel slow, even when the model itself was fast:

- **Persistent server** keeps the model loaded. No cold start on each request.
- **beam_size=1** uses greedy decoding instead of beam search. About 5x faster with no real accuracy difference for spoken dictation.
- **VAD filtering** trims silence from the start and end of recordings so the model only processes actual speech.
- **int8 quantization** cuts memory usage in half and speeds up inference.
- **ncat for IPC** instead of spawning a Python subprocess on every request.
- **jq for window detection** instead of a second Python subprocess.

## Uninstall

```bash
bash uninstall.sh
```

Removes the scripts and Python venv. Your config at `~/.config/hypr-dictate/` is left in place in case you want it later.

## License

MIT
