"""Owned compressed-codec fixtures and a visible Unicode subtitle sample."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
out=root/'BUILD/SAMPLES'
out.mkdir(exist_ok=True)
def ff(*args):
    return subprocess.check_output(['ffmpeg','-hide_banner','-loglevel','error',*args])
names=['PNG.AVI','ZMBV.AVI']
for name,codec,pixels in zip(names,['png','zmbv'],['rgb24','bgr0']):
    ff('-f','lavfi','-i','testsrc2=size=64x48:rate=6:duration=2',
       '-an','-c:v',codec,'-pix_fmt',pixels,'-threads','1','-y',str(out/name))
lines=['/* Independent host decode of zlib-dependent codecs. */',
       'static const char *media_names[]={"PNG.AVI","ZMBV.AVI"};',
       'static const unsigned long frame_hashes[][12]={']
for name in names:
    # Compare the lossless codecs in RGB; RGB-to-YUV matrix/range and
    # chroma-downsampling defaults differ between VLC and host FFmpeg.
    raw=ff('-i',str(out/name),'-pix_fmt','rgb24','-f','rawvideo','-')
    assert len(raw)==9216*12
    hashes=[]
    for offset in range(0,len(raw),9216):
        value=2166136261
        for byte in raw[offset:offset+9216]: value=((value^byte)*16777619)&0xffffffff
        hashes.append(f'0x{value:08x}UL')
    lines.append('{'+','.join(hashes)+'},')
lines.append('};')
base=(root/'BUILD/MEDDATA.H').read_text().splitlines()
lines.extend(line for line in base if line.startswith('#define AAC_'))
(root/'BUILD/ZDATA.H').write_text('\n'.join(lines)+'\n')
ff('-f','lavfi','-i','color=c=0x173b50:size=320x240:rate=5:duration=30',
   '-an','-c:v','libx264','-pix_fmt','yuv420p','-threads','1','-y',str(out/'SUBS.MP4'))
text='1\r\n00:00:01,000 --> 00:00:29,500\r\nRetroPlayer - Unicode subtitles\r\n'
text+='I\u011fd\u0131r / caf\u00e9 / \u0627\u0644\u0633\u0644\u0627\u0645\r\n\r\n'
(out/'SUBS.SRT').write_bytes(b'\xef\xbb\xbf'+text.encode('utf-8'))
ass='''[Script Info]
ScriptType: v4.00+
PlayResX: 320
PlayResY: 240
WrapStyle: 0
ScaledBorderAndShadow: yes

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,DejaVu Sans,22,&H00FFFFFF,&H0000FFFF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,1,1,2,10,10,20,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
'''
ass+=r'Dialogue: 0,0:00:01.00,0:00:29.50,Default,,0,0,0,,{\pos(160,180)\fad(250,250)\bord2}RetroPlayer {\b1}ASS{\b0}\N'
ass+='I\u011fd\u0131r / caf\u00e9 / \u0627\u0644\u0633\u0644\u0627\u0645\n'
(out/'SUBS.ASS').write_text(ass,encoding='utf-8')
print('Generated PNG/ZMBV references and a Unicode subtitle fixture')
