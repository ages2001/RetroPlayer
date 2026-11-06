"""Keep legacy imports ahead of libtool's C++ implicit runtime libraries."""
import os
import sys
from pathlib import Path

args = sys.argv[1:]
runtime = {f'/workspace/RPLIB/lib/{name}.a' for name in ('atom', 'kern', 'crt', 'net')}
expanded = []
for arg in args:
    expanded.extend([('-Wl,'+part) for part in arg[4:].split(',')]
                    if arg.startswith('-Wl,') else [arg])
reordered = []
found = False
i = 0
while i < len(expanded):
    if expanded[i] == '-Wl,--whole-archive':
        end = expanded.index('-Wl,--no-whole-archive', i+1)
        block = expanded[i+1:end]
        if block and all(a.removeprefix('-Wl,') in runtime for a in block):
            found = True
            i = end+1
            continue
    if expanded[i].removeprefix('-Wl,') not in runtime:
        reordered.append(expanded[i])
    i += 1
prefix = ['i686-w64-mingw32-g++', '-B/workspace/MWBLD/lib32/',
          '-include', '/workspace/RETRO/SRC/SYNCABI.H']
if os.environ.get('RP_BUILD_CXX') != '1' and Path('/workspace/CXXLIB/lib/libstdc++.a').is_file():
    prefix += ['-L/workspace/CXXLIB/lib', '-nostdinc++',
               '-isystem', '/workspace/CXXLIB/include/c++/14',
               '-isystem', '/workspace/CXXLIB/include/c++/14/i686-w64-mingw32',
               '-isystem', '/workspace/CXXLIB/include/c++/14/backward']
compile_only = any(a in ('-c', '-E', '-S') or a.startswith('-M') for a in args)
if found and not compile_only:
    prefix += ['-Wl,--whole-archive,'+','.join(sorted(runtime))+',--no-whole-archive']
os.execvp(prefix[0], prefix+reordered+([] if compile_only else ['/workspace/RPLIB/lib/atom.a']))
