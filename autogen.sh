#!/bin/sh

gnulib-tool --update
aclocal -I m4
autoconf
autoheader
automake -ac
