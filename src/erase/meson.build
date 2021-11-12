libeStoragedErase_lib = static_library(
  'libeStoragedErase-lib',
  'verifyDriveGeometry.cpp',
  'pattern.cpp',
  'erase.cpp',
  include_directories : eStoraged_headers,
  implicit_include_directories: false,
  dependencies: [eStoraged_dbus, dependency('stdplus'),],
)

libeStoragedErase_dep = declare_dependency(
  include_directories: eStoraged_headers,
  link_with: libeStoragedErase_lib,
)

