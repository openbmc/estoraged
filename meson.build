project(
    'eStoraged',
    'cpp',
    version: '1.0', meson_version: '>=0.57.0',
    default_options: [
      'warning_level=3',
      'werror=true',
      'cpp_std=c++20',
    ],
)

eStoraged_root = meson.current_source_dir()
eStoraged_dbus_headers = include_directories('.')

subdir('include')
subdir('xyz/openbmc_project/eStoraged')
subdir('src')
