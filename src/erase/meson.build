libeStoragedErase_lib = static_library(
  'libeStoragedErase-lib',
  'verifyDriveGeometry.cpp',
  'erase.cpp',
  'zero.cpp',
  include_directories : eStoraged_headers,
  implicit_include_directories: false,
  dependencies: [
    dependency('phosphor-dbus-interfaces'),
    dependency('stdplus'),
  ],
)

libeStoragedErase_dep = declare_dependency(
  include_directories: eStoraged_headers,
  link_with: libeStoragedErase_lib,
)

