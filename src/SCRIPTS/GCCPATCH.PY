"""Record the reviewed GCC runtime changes against the verified source."""
import difflib
from pathlib import Path

files = ['libstdc++-v3/configure.host', 'libstdc++-v3/include/std/mutex',
         'libstdc++-v3/src/c++11/chrono.cc']
root = Path(__file__).resolve().parent.parent
patch = []
for name in files:
    patch.extend(difflib.unified_diff(
        (Path('/workspace/GCCREF')/name).read_text().splitlines(True),
        (Path('/workspace/GCCWORK')/name).read_text().splitlines(True),
        fromfile='a/'+name, tofile='b/'+name))
(root/'PATCHES/GCC.DIF').write_text(''.join(patch))
