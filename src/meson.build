phosphor_dbus_interfaces_dep = dependency('phosphor-dbus-interfaces')
phosphor_logging_dep = dependency('phosphor-logging')

sdbusplus_dep = dependency('sdbusplus', required: false, include_type: 'system')
if not sdbusplus_dep.found()
  sdbusplus_proj = subproject('sdbusplus', required: true)
  sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
  sdbusplus = sdbusplus.as_system('system')
endif

stdplus_dep = dependency('stdplus')

boost_dep = dependency('boost',version : '>=1.78.0', required : false, include_type: 'system')
if not boost_dep.found()
  subproject('boost', required: false)
  boost_inc = include_directories('subprojects/boost_1_78_0/', is_system:true)
  boost  = declare_dependency(include_directories : boost_inc)
  boost = boost.as_system('system')
endif

subdir('erase')

libeStoraged_deps = [
  dependency('libcryptsetup'),
  dependency('openssl'),
  phosphor_dbus_interfaces_dep,
  phosphor_logging_dep,
  sdbusplus_dep,
  stdplus_dep,
  boost_dep,
]

libeStoraged_lib = static_library(
  'eStoraged-lib',
  'estoraged.cpp',
  'util.cpp',
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

