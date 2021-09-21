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

subdir('include/')
subdir('xyz/openbmc_project/eStoraged')
subdir('src/')

conf_data = configuration_data()
conf_data.set('libexecdir', get_option('prefix') / get_option('libexecdir'))

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
