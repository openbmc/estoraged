phosphor_dbus_interfaces_dep = dependency('phosphor-dbus-interfaces')
phosphor_logging_dep = dependency('phosphor-logging')

sdbusplus_dep = dependency('sdbusplus')
stdplus_dep = dependency('stdplus')

boost_dep = dependency('boost', version: '>=1.78.0', include_type: 'system')

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
    'getConfig.cpp',
    include_directories: eStoraged_headers,
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
    install_dir: get_option('bindir'),
)

build_tests = get_option('tests')
if build_tests.allowed()
    subdir('test')
endif
