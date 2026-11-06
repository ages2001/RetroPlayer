"""Stage the pinned, separately installed HDA drivers and verify every binary."""
import hashlib
import json
import runpy
import shutil
import subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[1]
for manifest,checkout,stage,target,prefix in (
    ('WINAUD.JSN','WINAUDIO','WINAUDIO/bin','WIN9X','WIN9X/'),
    ('AUDIONT.JSN','AUDIONT','ANTDIST','NT','')):
    meta=json.loads((root/'SOURCES'/manifest).read_text())
    source=Path('/workspace')/checkout
    commit=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
    if commit!=meta['commit']:raise SystemExit('Unexpected driver revision: '+checkout)
    archive=root/'SOURCES'/meta['archive']
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=meta['sha256']:
        raise SystemExit('Source archive checksum failed: '+manifest)
    entries=[]
    for record in meta['files']:
        if not record['file'].startswith(prefix):continue
        relative=Path(record['file'][len(prefix):])
        if relative.is_absolute() or '..' in relative.parts:raise SystemExit('Invalid manifest path')
        path=Path('/workspace')/stage/record['file']
        if hashlib.sha256(path.read_bytes()).hexdigest()!=record['sha256']:
            raise SystemExit('Driver checksum failed: '+str(path))
        entries.append((path,relative))
    dest=root/'DRIVERS'/target
    dest.mkdir(parents=True,exist_ok=True)
    for path,relative in entries:
        output=dest/relative;output.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(path,output)
    shutil.copyfile(source/'LICENSE',dest/'LICENSE.TXT')
    print(target+': '+str(len(entries))+' verified pinned files')
runpy.run_path(str(root/'SCRIPTS'/'AUDIT.PY'))['audit_names'](root/'DRIVERS')
