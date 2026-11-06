#!/bin/sh
# The CRT startup uses the same i386 atomic ABI as the engine.
for option do
  case "$option" in
    -c|-E|-S|-M*|--version|-dump*|-print*|-v|'-###')
      exec i686-w64-mingw32-gcc -B/workspace/MWBLD/lib32/ -include /workspace/RETRO/SRC/SYNCABI.H "$@" ;;
  esac
done
exec i686-w64-mingw32-gcc -B/workspace/MWBLD/lib32/ -include /workspace/RETRO/SRC/SYNCABI.H "$@" \
  /workspace/RPLIB/lib/atom.a
