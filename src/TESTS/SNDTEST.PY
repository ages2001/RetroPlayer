"""Positive and damaged-capture checks for the independent HDA analyser."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest
import wave
import numpy as np

class CaptureChecks(unittest.TestCase):
    def analyse(self,frequencies=(441,735,315),missing=False,clip=False,one_side=False,drop=False):
        rate=44100
        signal=np.zeros(rate*10)
        for index,freq in enumerate(frequencies):
            if missing and index==2:continue
            offset=rate*(1+index*3)
            t=np.arange(rate*2)/rate
            signal[offset:offset+len(t)]=np.sin(2*np.pi*freq*t)*(40000 if clip else 5000)
        if drop:signal[rate*4+rate//2:rate*4+rate//2+rate//10]=0
        pcm=np.clip(signal,-32768,32767).astype('<i2')
        stereo=np.column_stack([pcm,np.zeros_like(pcm) if one_side else pcm])
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'TEST.WAV';output=Path(directory)/'TEST.JSN'
            with wave.open(str(source),'wb') as out:
                out.setparams((2,2,rate,0,'NONE','not compressed'));out.writeframes(stereo.tobytes())
            run=subprocess.run([sys.executable,str(Path(__file__).resolve().parents[1]/'SCRIPTS/SNDCHK.PY'),
                                str(source),'--output',str(output)],capture_output=True,text=True)
            self.assertTrue(output.exists(),run.stderr)
            return run.returncode,json.loads(output.read_text())
    def test_known_good_sequence(self):
        code,data=self.analyse();self.assertEqual(code,0,data);self.assertEqual(data['result'],'PASS')
    def test_missing_tone(self):self.assertNotEqual(self.analyse(missing=True)[0],0)
    def test_wrong_pitch(self):self.assertNotEqual(self.analyse(frequencies=(460,735,315))[0],0)
    def test_clipping(self):self.assertNotEqual(self.analyse(clip=True)[0],0)
    def test_dead_channel(self):self.assertNotEqual(self.analyse(one_side=True)[0],0)
    def test_dropout(self):self.assertNotEqual(self.analyse(drop=True)[0],0)

if __name__=='__main__':unittest.main(verbosity=2)
