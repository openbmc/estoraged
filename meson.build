project(
    'eStoraged',
    'cpp',
    version: '1.0', meson_version: '>=0.57.0',
    default_options: [
        'werror'=true,
        'cpp_std=c++17',
    ],
)

cpp = meson.get_compiler('cpp')

eStoraged_root = meson.current_source_dir()
eStoraged_headers = include_directories('.')
subdir('xyz/openbmc_project/eStoraged')

libestoraged_deps = [
  dependency('sdbusplus'),
  dependency('phosphor-logging'),
  eStoraged_dbus,
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
  implicit_indluce_directories: false,
  dependencies: libestoraged,
  install: true,
  install_dir: get_option('bindir')
)
