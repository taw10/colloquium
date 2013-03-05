#!/bin/sh

libtoolize --force --copy \
&& aclocal -I m4 --force \
&& autoheader --force \
&& automake --add-missing --copy --force \
&& autoconf --force

pushd harfatum
./autogen.sh
popd
