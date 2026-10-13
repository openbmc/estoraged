
eStoraged_headers = include_directories('.')

cxx = meson.get_compiler('cpp')
sdbusplus_dep = dependency('sdbusplus')
sdbusplus_includes = sdbusplus_dep.partial_dependency(
    includes: true,
    compile_args: true,
)

conf_data = configuration_data()
# If the number of drives increases we plan to switch to a config file design
# in which, the max and min size are read and parsed for a config file at
# verification time.
conf_data.set('ERASE_MAX_GEOMETRY', get_option('erase_max_geometry'))
conf_data.set('ERASE_MIN_GEOMETRY', get_option('erase_min_geometry'))
conf_data.set10(
    'HAVE_SDBUSPLUS_OBJECT_PATH',
    cxx.has_type(
        'sdbusplus::object_path',
        prefix: '#include <sdbusplus/message/native_types.hpp>',
        dependencies: sdbusplus_includes,
    ),
)
conf_data.set10(
    'HAVE_SDBUSPLUS_MATCH',
    cxx.has_type(
        'sdbusplus::match',
        prefix: '#include <sdbusplus/bus/match.hpp>',
        dependencies: sdbusplus_includes,
    ),
)
configure_file(
    input: 'config.h.in',
    output: 'estoraged_conf.hpp',
    configuration: conf_data,
)
