#!/bin/sh
# The CRT startup uses the same i386 atomic ABI as the engine.
for option do
  case "$option" in
    --version|-dump*|-print*|-v|'-###')
      exec i686-w64-mingw32-g++ -B/workspace/MWBLD/lib32/ "$@" ;;
  esac
done
exec python3 /workspace/RETRO/SCRIPTS/CXXLINK.PY "$@"
