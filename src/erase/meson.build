libeStoragedErase_lib = static_library(
  'libeStoragedErase-lib',
  'verifyDriveGeometry.cpp',
  'findSize.cpp',
  include_directories : eStoraged_headers,
  implicit_include_directories: false,
  dependencies: eStoraged_dbus,
)

libeStoragedErase_dep = declare_dependency(
  include_directories: eStoraged_headers,
  link_with: libeStoragedErase_lib,
  )

