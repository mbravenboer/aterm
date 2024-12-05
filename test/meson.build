
terms_gen = custom_target('terms',
    input : 'terms.dict',
    output : ['terms.c', 'terms.h'],
    command : [dicttoc_exe,
        '-file-name', 'terms',
        '-file-prefix', '@OUTDIR@/terms',
        '-dict', '@INPUT@'])

stress_exe = executable('stress', ['stress.c', terms_gen[1], terms_gen[0]],
    include_directories : aterm_include_dirs,
    link_with : aterm_library)

test('stress', stress_exe)


aterm_tests = {
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
