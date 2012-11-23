#!/bin/sh

libtoolize --force --copy \
&& aclocal -I m4 --force \
&& autoheader --force \
&& automake --add-missing --copy --force \
&& autoconf --force

pushd libstorycode
./autogen.sh
popd

pushd harfatum
./autogen.sh
popd
