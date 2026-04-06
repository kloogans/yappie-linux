project('tomlc99', 'c', version: '1.0', license: 'MIT')

tomlc99_lib = static_library('tomlc99', 'toml.c',
  include_directories: include_directories('.'),
  c_args: ['-std=c99'],
)

tomlc99_dep = declare_dependency(
  link_with: tomlc99_lib,
  include_directories: include_directories('.'),
)
