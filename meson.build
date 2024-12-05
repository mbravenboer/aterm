project('aterm', 'c',
  version : '25.01',
  license : 'BSD',
  default_options : ['default_library=both'])

subdir('src')
subdir('utils')
subdir('test')

pkg_mod = import('pkgconfig')

pkg_mod.generate(libraries : aterm_library,
                 version : '25.01',
                 name : 'libaterm',
                 filebase : 'aterm',
                 description : 'ATerm Library')
