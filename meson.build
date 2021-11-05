project(
    'eStoraged',
    'cpp',
    version: '1.0',
    default_options: [
        'cpp_std=c++20',
    ],
)

cpp = meson.get_compiler('cpp')

sdbusplus_dep = dependency('sdbusplus',required : false)
if sdbusplus_dep.found() and sdbusplus_dep.type_name() != 'internal'
  sdbusplus_prog = find_program('sdbus++', native: true)
else
  sdbusplus_proj = subproject('sdbusplus', required: true)
  sdbusplus_dep = sdbusplus_proj.get_variable('sdbusplus_dep')
  sdbusplus_dep = sdbusplus_dep.as_system('system')
  sdbusplus_prog = sdbusplus_proj.get_variable('sdbusplusplus_prog')
endif

eStoraged_root = meson.current_source_dir()
eStoraged_headers = include_directories('erase', '.')

subdir('xyz/openbmc_project/eStoraged')

libestoraged_deps = [
  eStoraged_dbus,
]
libestoraged_deps += sdbusplus_dep

libestoraged_lib = static_library(
  'estoraged',
  'estoraged.cpp',
  'erase/verifyGeometry.cpp',
  'erase/findSize.cpp',
  'erase/sanitize.cpp',
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
  install_dir: get_option('libexecdir')
)

if not get_option('tests').disabled()
  subdir('test')
endif

conf_data = configuration_data()
conf_data.set('libexecdir', get_option('prefix') / get_option('libexecdir'))
conf_data.set('datadir', get_option('prefix') / get_option('datadir'))
conf_data.set('ERASE_MAX_GEOMETRY', get_option('erase_max_geometry'))
conf_data.set('ERASE_MIN_GEOMETRY', get_option('erase_min_geometry'))

systemd_dep = dependency('systemd')
if systemd_dep.found()
  unit_dir = systemd_dep.get_variable(pkgconfig: 'systemdsystemunitdir')
  rule_dir = dependency('udev').get_variable(pkgconfig: 'udev_dir') / 'rules.d'

  install_data(
    '70-estoraged.rules',
    install_dir: rule_dir)

  configure_file(
    input: 'estoraged@.service.in',
    output: 'estoraged@.service',
    configuration: conf_data,
    install: true,
    install_dir: unit_dir)
endif

configure_file(
  input: 'estoraged_conf.hpp.in',
  output: 'estoraged_conf.hpp',
  configuration: conf_data)
