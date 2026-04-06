project('cjson', 'c', version: '1.7.18', license: 'MIT')

cjson_lib = static_library('cjson', 'cJSON.c',
  include_directories: include_directories('.'),
  c_args: ['-std=c99', '-DCJSON_HIDE_SYMBOLS'],
)

cjson_dep = declare_dependency(
  link_with: cjson_lib,
  include_directories: include_directories('.'),
)
