#! /usr/bin/env bash

set -e
set -u

prefix=/opt/stratego/aterm
builddir=build

rm -rf $builddir

set -x
meson setup $builddir --prefix=$prefix --buildtype=release

pushd $builddir
meson compile --verbose
meson test
meson install
popd

