sdbuspp_prog = find_program('sdbus++')

domain = 'xyz.openbmc_project.eStoraged'
if_yaml_file = files('../eStoraged.interface.yaml')

if_cpp = custom_target(
  'server.cpp',
  output: 'server.cpp',
  input: if_yaml_file,
  capture: true,
  command: [sdbuspp_prog, '-r', eStoraged_root, 'interface', 'server-cpp', domain])

if_hpp = custom_target(
  'server.hpp',
  output: 'server.hpp',
  input: if_yaml_file,
  capture: true,
  command: [sdbuspp_prog, '-r', eStoraged_root, 'interface', 'server-header', domain],
  install: true,
  install_dir: get_option('includedir') / 'xyz/openbmc_project/eStoraged')

error_yaml_file = files('../eStoraged.errors.yaml')

error_cpp = custom_target(
  'error.cpp',
  output: 'error.cpp',
  input: error_yaml_file,
  capture: true,
  command: [sdbuspp_prog, '-r', eStoraged_root, 'error', 'exception-cpp', domain])

error_hpp = custom_target(
  'error.hpp',
  output: 'error.hpp',
  input: error_yaml_file,
  capture: true,
  command: [sdbuspp_prog, '-r', eStoraged_root, 'error', 'exception-header', domain],
  install: true,
  install_dir: get_option('includedir') / 'xyz/openbmc_project/eStoraged')

eStoraged_dbus_deps = [
  dependency('sdbusplus'),
]

eStoraged_dbus_lib = library(
  'eStoraged-dbus',
  if_cpp,
  if_hpp,
  error_cpp,
  error_hpp,
  implicit_include_directories: false,
  include_directories: eStoraged_dbus_headers,
  version: meson.project_version(),
  dependencies: eStoraged_dbus_deps,
  install: true)

eStoraged_dbus = declare_dependency(
  dependencies: eStoraged_dbus_deps,
  sources: [if_hpp, error_hpp],
  include_directories: eStoraged_dbus_headers,
  link_with: eStoraged_dbus_lib)

eStoraged_dbus_reqs = []
foreach dep : eStoraged_dbus_deps
  if dep.type_name() == 'pkgconfig'
    eStoraged_dbus_reqs += dep
  endif
endforeach

import('pkgconfig').generate(
  name: 'eStoraged-dbus',
  description: 'eStoraged DBus Bindings',
  version: meson.project_version(),
  libraries: eStoraged_dbus,
  requires: eStoraged_dbus_reqs)
