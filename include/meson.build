
eStoraged_headers = include_directories('.')

conf_data = configuration_data()
# If the number of drives increases we plan to switch to a config file design
# in which, the max and min size are read and parsed for a config file at
# verification time.
conf_data.set('ERASE_MAX_GEOMETRY', get_option('erase_max_geometry'))
conf_data.set('ERASE_MIN_GEOMETRY', get_option('erase_min_geometry'))

configure_file(
  input: 'estoraged_conf.hpp.in',
  output: 'estoraged_conf.hpp',
  configuration: conf_data)
