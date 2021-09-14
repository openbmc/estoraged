
libestoraged_deps = [
  dependency('phosphor-dbus-interfaces',
    fallback: ['phosphor-dbus-interfaces', 'phosphor_dbus_interfaces_dep']),
]

libestoraged_lib = static_library(
  'estoraged',
  'estoraged.cpp',
  include_directories : eStoraged_headers,
  implicit_include_directories: false,
  dependencies: libestoraged_deps,
)

libestoraged = declare_dependency(
  dependencies: libestoraged_deps,
  include_directories: eStoraged_headers,
  link_with: libestoraged_lib)

executable(
  'eStoraged',
  'main.cpp',
  implicit_include_directories: false,
  dependencies: libestoraged,
  install: true,
  install_dir: get_option('bindir')
)
