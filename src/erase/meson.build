libeStoragedErase_lib = static_library(
    'libeStoragedErase-lib',
    'verifyDriveGeometry.cpp',
    'pattern.cpp',
    'cryptoErase.cpp',
    'sanitize.cpp',
    'zero.cpp',
    include_directories: eStoraged_headers,
    implicit_include_directories: false,
    dependencies: [
        phosphor_dbus_interfaces_dep,
        phosphor_logging_dep,
        stdplus_dep,
        boost_dep,
    ],
)

libeStoragedErase_dep = declare_dependency(
    include_directories: eStoraged_headers,
    link_with: libeStoragedErase_lib,
)

