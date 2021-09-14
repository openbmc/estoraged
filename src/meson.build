
libestoraged_deps = [
  dependency('sdbusplus'),
  eStoraged_dbus,
]

libestoraged_lib = static_library(
  'estoraged',
  'estoraged.cpp',
  include_directories : [eStoraged_headers, eStoraged_dbus_headers],
  implicit_include_directories: false,
  dependencies: libestoraged_deps,
)

libestoraged = declare_dependency(
  dependencies: libestoraged_deps,
  include_directories: [eStoraged_headers, eStoraged_dbus_headers],
  link_with: libestoraged_lib)

executable(
  'eStoraged',
  'main.cpp',
  implicit_include_directories: false,
  dependencies: libestoraged,
  install: true,
  install_dir: get_option('bindir')
)
