
sources = files(
    'afun.c',
    'aterm.c',
    'byteencoding.c',
    'gc.c',
    'hash.c',
    'list.c',
    'make.c',
    'memmgnt.c',
    'md5c.c',
    'memory.c',
    'byteio.c',
    'bafio.c',
    'safio.c',
    'tafio.c',
    'version.c',
    'idmappings.c')

aterm_include_dirs = [include_directories('.')]

public_headers = files(
  'abool.h',
  'afun.h',
  'aterm1.h',
  'aterm2.h',
  'atypes.h',
  'deprecated.h',
  'encoding.h',
  'safio.h',
  'idmappings.h')

install_headers(public_headers, subdir: 'aterm')

aterm_library = library(
  'aterm',
  sources,
  include_directories : aterm_include_dirs,
  link_args : ['-Wl,-no-undefined'],
  install: true)
