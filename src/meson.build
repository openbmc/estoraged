
libeStoraged_deps = [
  dependency('sdbusplus'),
  eStoraged_dbus,
]

phosphor_logging_dep = dependency(
    'phosphor-logging',
    fallback: ['phosphor-logging', 'phosphor_logging_dep'],
)

libeStoraged_lib = static_library(
  'eStoraged-lib',
  'estoraged.cpp',
  include_directories : eStoraged_headers,
  implicit_include_directories: false,
  dependencies: [libeStoraged_deps, phosphor_logging_dep],
)

libeStoraged = declare_dependency(
  dependencies: libeStoraged_deps,
  include_directories: eStoraged_headers,
  link_with: libeStoraged_lib)

executable(
  'eStoraged',
  'main.cpp',
  implicit_include_directories: false,
  dependencies: libeStoraged,
  install: true,
  install_dir: get_option('bindir')
)
