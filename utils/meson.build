libdir = join_paths(
  get_option('prefix'),
  get_option('libdir'))

dicttoc_exe = executable('dicttoc', 'dicttoc.c',
    include_directories : aterm_include_dirs,
    link_with : aterm_library,
    install: true,
    install_rpath : libdir)

atdiff_exe = executable('atdiff', 'atdiff.c',
    include_directories : aterm_include_dirs,
    link_with : aterm_library,
    install: true,
    install_rpath : libdir)

baffle_exe = executable('baffle', 'baffle.c',
    include_directories : aterm_include_dirs,
    link_with : aterm_library,
    install: true,
    install_rpath : libdir)
