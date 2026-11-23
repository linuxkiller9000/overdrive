#!/bin/sh
# OVERDRIVE prototype launcher (Linux)
# The binary is statically linked against libstdc++/libgcc; it needs only
# libc, libm and a working OpenGL 2.1+ driver (X11 or Wayland session).
cd "$(dirname "$0")"
exec ./overdrive "$@"
