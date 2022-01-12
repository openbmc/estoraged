gtest = dependency('gtest', main: true, disabler: true, required: build_tests)
gmock = dependency('gmock', disabler: true, required: build_tests)

tests = [
  'erase/verifyGeometry_test',
  'erase/pattern_test',
  'estoraged_test',
  # 'luks_test',
]

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     dependencies: [
                       gtest,
                       gmock,
                       libeStoraged,
                     ],
                     include_directories: eStoraged_headers),
	  timeout: 180)
endforeach
