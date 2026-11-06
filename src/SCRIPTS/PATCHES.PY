"""Record the reviewed engine source changes against the pinned VLC tree."""
import difflib
from pathlib import Path
import subprocess

reference = Path('/workspace/VLCREF')
working = Path('/workspace/VLCWORK')
project = Path(__file__).resolve().parent.parent
files = ['Makefile.am', 'configure.ac', 'src/Makefile.am', 'lib/Makefile.am',
         'modules/common.am', 'modules/video_chroma/Makefile.am',
         'modules/video_output/Makefile.am', 'modules/video_output/RPGDI.c',
         'modules/video_output/win32/win32touch.h', 'modules/video_output/win32/win32touch.c',
         'src/video_output/vout_internal.h', 'src/video_output/video_output.c',
         'src/audio_output/aout_internal.h', 'src/audio_output/dec.c',
         'modules/audio_output/waveout.c', 'modules/codec/avcodec/audio.c',
         'modules/audio_filter/resampler/ugly.c', 'modules/audio_filter/resampler/speex.c',
         'modules/demux/ogg.c', 'modules/demux/mp4/mp4.c', 'modules/demux/avformat/demux.c', 'modules/demux/asf/asf.c', 'modules/demux/mpeg/ts.c',
         'modules/codec/opus.c', 'modules/codec/vorbis.c',
         'modules/text_renderer/Makefile.am', 'modules/text_renderer/freetype/freetype.c',
         'modules/text_renderer/freetype/platform_fonts.c', 'modules/text_renderer/freetype/platform_fonts.h',
         'modules/text_renderer/freetype/fonts/RPFONTS.c',
         'modules/codec/Makefile.am', 'modules/codec/libass.c', 'modules/codec/subsdec.c', 'src/input/decoder.c', 'src/input/es_out.c',
         'test/Makefile.am', 'include/vlc_fixups.h', 'src/vlc-plugin.pc.in', 'src/win32/thread.c', 'src/win32/RPCLOCK.H',
         'src/win32/RPLOAD.H', 'src/win32/filesystem.c', 'src/win32/dirs.c', 'src/win32/plugin.c', 'compat/poll.c',
         'src/misc/interrupt.c', 'src/misc/block.c', 'src/win32/specific.c', 'src/win32/rand.c',
         'include/RPTRACE.H', 'lib/core.c', 'src/libvlc.c', 'src/misc/objects.c', 'src/misc/variables.c', 'src/modules/bank.c', 'src/modules/cache.c', 'modules/access/file.c']
commit = subprocess.check_output(['git', '-C', str(reference), 'rev-parse', 'HEAD'], text=True).strip()
if commit != '6de05adcbaf2e8b85fe86aad4169393098628119':
    raise SystemExit('Unexpected upstream revision; refusing to create a patch')
if subprocess.check_output(['git', '-C', str(reference), 'status', '--porcelain']):
    raise SystemExit('Upstream reference is modified; refusing to create a patch')
patch = []
for name in files:
    patch.extend(difflib.unified_diff(
        (reference/name).read_text().splitlines(keepends=True) if (reference/name).exists() else [],
        (working/name).read_text().splitlines(keepends=True),
        fromfile='a/'+name if (reference/name).exists() else '/dev/null', tofile='b/'+name))
(project/'PATCHES').mkdir(exist_ok=True)
(project/'PATCHES'/'LEGACY.DIF').write_text(''.join(patch))
print(f'Recorded source changes against {commit}')
