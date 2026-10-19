#!/usr/bin/env python3
"""Measure the portable translation/protocol core; no motor or vendor app access."""
import json
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'coverage'
OUT.mkdir(exist_ok=True)
def run(*args,**kw):
    return subprocess.run([str(x) for x in args],check=True,**kw)
names=['report_map','force_protocol','wire_protocol']
objects=[]; profiles=[]
for name in names:
    binary=OUT/name; profile=OUT/(name+'.profraw')
    run('clang','-O0','-g','-fprofile-instr-generate','-fcoverage-mapping','-I',ROOT/'source',ROOT/'tests'/('test_'+name+'.c'),'-o',binary)
    run(binary,env={**os.environ,'LLVM_PROFILE_FILE':str(profile)})
    objects.append(binary);profiles.append(profile)
merged=OUT/'core.profdata'
run('xcrun','llvm-profdata','merge','-sparse',*profiles,'-o',merged)
args=['xcrun','llvm-cov','export',str(objects[0]),'-instr-profile='+str(merged)]
for obj in objects[1:]:args+=['-object',str(obj)]
result=run(*args,capture_output=True,text=True)
data=json.loads(result.stdout)
headers={'report_map.h','ffb_protocol.h','ffb_wire.h'}
files=[f for d in data['data'] for f in d['files'] if Path(f['filename']).name in headers]
assert len(files)==3, 'Missing coverage for a core header'
summary={}
for metric,minimum in [('lines',95),('branches',90)]:
    count=sum(f['summary'][metric]['count'] for f in files)
    covered=sum(f['summary'][metric]['covered'] for f in files)
    percent=100*covered/count if count else 100
    summary[metric]={'covered':covered,'count':count,'percent':round(percent,2),'minimum':minimum}
    assert percent>=minimum, (metric,percent,minimum)
# Publish relative paths only.
for d in data['data']:
    for f in d['files']: f['filename']=str(Path(f['filename']).relative_to(ROOT))
    for function in d.get('functions',[]):
        function['filenames']=[str(Path(p).relative_to(ROOT)) for p in function.get('filenames',[])]
(OUT/'coverage.json').write_text(json.dumps(data,indent=2)+'\n')
(OUT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
