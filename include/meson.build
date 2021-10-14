
eStoraged_headers = include_directories('.')

conf_data = configuration_data()
conf_data.set('ERASE_MAX_GEOMETRY', get_option('erase_max_geometry'))
conf_data.set('ERASE_MIN_GEOMETRY', get_option('erase_min_geometry'))

configure_file(
  input: 'estoraged_conf.hpp.in',
  output: 'estoraged_conf.hpp',
  configuration: conf_data)
