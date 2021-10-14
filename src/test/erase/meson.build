gtest = dependency('gtest', main: true, disabler: true, required: get_option('tests'))
gmock = dependency('gmock', disabler: true, required: get_option('tests'))

if not gtest.found() or not gmock.found()
  gtest_proj = import('cmake').subproject('googletest', required: false)
  if gtest_proj.found()
    gtest = declare_dependency(
      dependencies: [
        gtest_proj.dependency('gtest'),
        gtest_proj.dependency('gtest_main'),
      ])
    gmock = gtest_proj.dependency('gmock')
  else
    assert(not get_option('tests').enabled(), 'Googletest is required')
  endif
endif

gtests = [
  'verifyGeometry',
  ]

foreach t : gtests
  test(t, executable(t.underscorify(), t + '_test.cpp',
                     build_by_default: false,
                     include_directories : eStoraged_headers,
                     implicit_include_directories: false,
		     link_with: libeStoraged_lib,
                     dependencies: [gtest, gmock, eStoraged_dbus]))
endforeach
