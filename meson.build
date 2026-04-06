project('yappie', 'c',
  version: '0.5.0',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

add_project_arguments('-D_GNU_SOURCE', language: 'c')
cc = meson.get_compiler('c')

# ---------- System dependencies ----------

pipewire_dep = dependency('libpipewire-0.3', required: true)
curl_dep     = dependency('libcurl', required: true)
math_dep     = cc.find_library('m', required: true)
pthread_dep  = dependency('threads', required: true)

systemd_dep = dependency('libsystemd', required: get_option('systemd'))
if systemd_dep.found()
  add_project_arguments('-DHAVE_SYSTEMD', language: 'c')
endif

# ---------- Whisper.cpp (optional — needed for local backend) ----------

whisper_opt = get_option('whisper')
whisper_dep = dependency('', required: false)  # empty fallback

if whisper_opt.disabled()
  whisper_found = false
else
  # Try pkg-config first, then raw library search
  whisper_dep = dependency('whisper', required: false)
  if not whisper_dep.found()
    whisper_lib = cc.find_library('whisper', required: false)
    whisper_header = cc.has_header('whisper.h')
    if whisper_lib.found() and whisper_header
      whisper_dep = declare_dependency(dependencies: whisper_lib)
    endif
  endif
  whisper_found = whisper_dep.found()
  if whisper_opt.enabled() and not whisper_found
    error('whisper.cpp requested but not found. Install whisper.cpp or pass -Dwhisper=disabled')
  endif
endif

if whisper_found
  add_project_arguments('-DHAVE_WHISPER', language: 'c')
endif

# ---------- Vendored subprojects ----------

tomlc99_dep = subproject('tomlc99').get_variable('tomlc99_dep')
cjson_dep   = subproject('cjson').get_variable('cjson_dep')

# ---------- Common include ----------

common_inc = include_directories('src/common')
daemon_inc = include_directories('src/daemon')

# ---------- Shared IPC library (used by both daemon and CLI) ----------

ipc_lib = static_library('yappie-ipc', 'src/daemon/ipc.c',
  include_directories: [common_inc, daemon_inc],
)
ipc_dep = declare_dependency(
  link_with: ipc_lib,
  include_directories: [common_inc, daemon_inc],
)

# ---------- Daemon ----------

daemon_sources = [
  'src/daemon/main.c',
  'src/daemon/state.c',
  'src/daemon/config.c',
  'src/daemon/audio.c',
  'src/daemon/backend.c',
  'src/daemon/api_backend.c',
  'src/daemon/tcp_backend.c',
  'src/daemon/wav.c',
  'src/daemon/paste.c',
  'src/daemon/local_backend.c',
  'src/daemon/model.c',
]

daemon_deps = [
  pipewire_dep,
  curl_dep,
  math_dep,
  pthread_dep,
  tomlc99_dep,
  cjson_dep,
]

if systemd_dep.found()
  daemon_deps += systemd_dep
endif

if whisper_found
  daemon_deps += whisper_dep
endif

executable('yappied', daemon_sources,
  dependencies: daemon_deps,
  link_with: ipc_lib,
  include_directories: [common_inc, daemon_inc],
  install: true,
)

# ---------- CLI ----------

cli_sources = [
  'src/cli/main.c',
]

executable('yappie', cli_sources,
  dependencies: [ipc_dep],
  include_directories: [common_inc, daemon_inc],
  install: true,
)

# ---------- Data files ----------

install_data('config.example.toml',
  install_dir: get_option('datadir') / 'yappie',
)

install_data('data/yappied.service',
  install_dir: get_option('prefix') / 'lib' / 'systemd' / 'user',
)
