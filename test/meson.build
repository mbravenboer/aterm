
aterm_tests = {
    'stress': ['stress.c', 'terms.c'],
    'primes': ['primes.c'],
    'randgen': ['randgen.c'],
    'fib': ['fib.c'],
    'testsafio': ['testsafio.c'],
}

foreach name, src: aterm_tests
  exec = executable(name, src,
    include_directories : aterm_include_dirs,
    link_with : aterm_library)
  test(name, exec)
endforeach
