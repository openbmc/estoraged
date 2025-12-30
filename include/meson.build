
eStoraged_headers = include_directories('.')


highspeed_parts_content = get_option('highspeed_parts')

highspeed_parts_entries = []
foreach item : highspeed_parts_content
    highspeed_parts_entries += '"@0@"'.format(item)
endforeach
highspeed_parts = ', '.join(highspeed_parts_entries)

if get_option('tests').allowed()
    highspeed_parts = '"TestPart"'
endif

conf_data = configuration_data()
# If the number of drives increases we plan to switch to a config file design
# in which, the max and min size are read and parsed for a config file at
# verification time.
conf_data.set('ERASE_MAX_GEOMETRY', get_option('erase_max_geometry'))
conf_data.set('ERASE_MIN_GEOMETRY', get_option('erase_min_geometry'))
conf_data.set('HIGHSPEED_PARTS', highspeed_parts)
configure_file(
    input: 'config.h.in',
    output: 'estoraged_conf.hpp',
    configuration: conf_data,
)
