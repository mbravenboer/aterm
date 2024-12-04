project('aterm', 'c',
  version : '25.01',
  license : 'BSD')

subdir('src')
subdir('test')

pkg_mod = import('pkgconfig')

pkg_mod.generate(libraries : aterm_library,
                 version : '25.01',
                 name : 'libaterm',
                 filebase : 'aterm',
                 description : 'ATerm Library')
