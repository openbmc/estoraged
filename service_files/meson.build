systemd_dep = dependency('systemd')

systemd_system_unitdir = systemd_dep.get_variable('systemdsystemunitdir')

conf_data = configuration_data()
conf_data.set('BINDIR', get_option('prefix') / get_option('bindir'))

configure_file(
    configuration: conf_data,
    input: 'xyz.openbmc_project.eStoraged.service.in',
    output: 'xyz.openbmc_project.eStoraged.service',
    install: true,
    install_dir: systemd_system_unitdir,
)
