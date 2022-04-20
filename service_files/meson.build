systemd_dep = dependency('systemd')
systemd_system_unitdir = systemd_dep.get_variable(
  pkgconfig: 'systemdsystemunitdir')

configure_file(
  copy: true,
  input: 'xyz.openbmc_project.eStoraged.service',
  install: true,
  install_dir: systemd_system_unitdir,
  output: 'xyz.openbmc_project.eStoraged.service'
)
