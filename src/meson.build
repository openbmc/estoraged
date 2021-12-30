
subdir('erase')

libeStoraged_deps = [
  dependency('sdbusplus'),
  dependency('phosphor-logging',
    fallback: ['phosphor-logging',
               'phosphor_logging_dep'],
    ),
  dependency('openssl'),
  dependency('libcryptsetup'),
  dependency('stdplus'),
  dependency('phosphor-dbus-interfaces'),
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

build_tests = get_option('tests')
if not build_tests.disabled()
  subdir('test')
endif

