phosphor_dbus_interfaces_dep = dependency(
  'phosphor-dbus-interfaces',
  fallback: ['phosphor-dbus-interfaces', 'phosphor_dbus_interfaces_dep'],
)

phosphor_logging_dep = dependency(
  'phosphor-logging',
  fallback: ['phosphor-logging', 'phosphor_logging_dep'],
)

sdbusplus_dep = dependency(
  'sdbusplus',
  fallback: ['sdbusplus', 'sdbusplus_dep'],
)

stdplus_dep = dependency(
  'stdplus',
  fallback: ['stdplus', 'stdplus_dep'],
)

subdir('erase')

libeStoraged_deps = [
  dependency('libcryptsetup'),
  dependency('openssl'),
  phosphor_dbus_interfaces_dep,
  phosphor_logging_dep,
  sdbusplus_dep,
  stdplus_dep,
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

