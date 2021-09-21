project(
    'eStoraged',
    'cpp',
    version: '1.0',
    default_options: [
        'cpp_std=c++17',
    ],
)

cpp = meson.get_compiler('cpp')

sdbusplus = dependency('sdbusplus',required : false)
if not sdbusplus.found()
  sdbusplus_proj = subproject('sdbusplus', required: true)
  sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
  sdbusplus = sdbusplus.as_system('system')
endif

eStoraged_root = meson.current_source_dir()
eStoraged_headers = include_directories('.')
subdir('xyz/openbmc_project/eStoraged')

libestoraged_deps = [
  eStoraged_dbus,
]
libestoraged_deps += sdbusplus

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
  install_dir: get_option('libexecdir')
)

conf_data = configuration_data()
conf_data.set('libexecdir', get_option('prefix') / get_option('libexecdir'))
conf_data.set('datadir', get_option('prefix') / get_option('datadir'))

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
