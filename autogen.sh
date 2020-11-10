#!/bin/sh

gettextize --f
aclocal -I m4
autoheader
automake
autoconf
