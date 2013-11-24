#!/bin/sh

test -d m4 || mkdir m4
gtkdocize || exit 1
autoreconf -ivf
