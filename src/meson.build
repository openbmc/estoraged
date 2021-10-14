
subdir('erase')

libeStoraged_deps = [
  dependency('sdbusplus'),
  dependency('phosphor-logging',
    fallback: ['phosphor-logging',
               'phosphor_logging_dep'],
    ),
  eStoraged_dbus,
]

libeStoraged_lib = static_library(
  'eStoraged-lib',
  'estoraged.cpp',
  include_directories : eStoraged_headers,
  implicit_include_directories: false,
  dependencies: [libeStoraged_deps, libeStoragedErase_dep],
)

libeStoraged = declare_dependency(
  dependencies: libeStoraged_deps,
  include_directories: eStoraged_headers,
  link_with: libeStoraged_lib,
)

executable(
  'eStoraged',
  'main.cpp',
  implicit_include_directories: false,
  dependencies: libeStoraged,
  install: true,
  install_dir: get_option('bindir')
)


if not get_option('tests').disabled()
  subdir('test')
endif

