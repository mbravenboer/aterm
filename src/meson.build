# lib_LTLIBRARIES = libATerm.la
# include_HEADERS = $(PUBLIC_INCL)
# noinst_HEADERS  = $(PRIVATE_INCL)

# libATerm_la_SOURCES = $(ALLSRCS)
# libATerm_la_CFLAGS = -DNDEBUG
# libATerm_la_LDFLAGS = -avoid-version -no-undefined

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

include_dirs = [include_directories('.')]

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

aterm_library = library(
  'aterm',
  sources,
  include_directories : include_dirs,
  install: true)

# PRIVATE_INCL = \
#   _afun.h \
#   _aterm.h \
#   bafio.h \
#   byteio.h \
#   debug.h \
#   gc.h \
#   list.h \
#   make.h \
#   md5.h \
#   memory.h \
#   tafio.h \
#   util.h \
#   version.h \
#   byteencoding.h
