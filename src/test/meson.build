gtest = dependency('gtest', main: true, disabler: true, required: build_tests)
gmock = dependency('gmock', disabler: true, required: build_tests)

tests = [
  'estoraged_test',
]

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     dependencies: [
                       gtest,
                       gmock,
		       libeStoraged,
		     ],
		     include_directories: eStoraged_headers))
endforeach
