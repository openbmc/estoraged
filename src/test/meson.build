gtest = dependency('gtest', main: true, disabler: true, required: build_tests)
gmock = dependency('gmock', disabler: true, required: build_tests)

tests = [
  'erase/verifyGeometry_test',
  'erase/pattern_test',
  'erase/zero_test',
  'erase/crypto_test',
  'erase/sanitize_test',
  'estoraged_test',
  'util_test',
]

test_eStoraged_headers = include_directories('include')

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     dependencies: [
                       gtest,
                       gmock,
                       libeStoraged,
                     ],
                     include_directories: [eStoraged_headers, test_eStoraged_headers]))
endforeach
