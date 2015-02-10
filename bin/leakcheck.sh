#!/bin/sh
OPTS="--tool=memcheck --leak-check=full --show-reachable=yes --leak-resolution=high --num-callers=20"

G_SLICE=always-malloc G_DEBUG=gc-friendly,resident-modules valgrind $OPTS --log-file=vgdump $1
