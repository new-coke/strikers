#!/usr/bin/env python3
"""Archive/hash verified finite captures, then remove only reused test files."""
from pathlib import Path
import json,sys,time
BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[1]
sys.path.insert(0,str(ROOT/'smstrikers-port/tools'))
import vita_workflow_device as d
out = BASE/sys.argv[1]
identity = json.loads((out/'capture-identity.json').read_text())
analysis = json.loads((out/'analysis.json').read_text())
if not analysis['all_gameplay'] or not analysis['live_play_only']: raise SystemExit('Invalid capture')
r={'time_unix':time.time(),'destroy':d.command('192.168.1.79','destroy'),'files':{},'deleted':[]}
f=d.connect('192.168.1.79')
try:
    binary=d.read(f,d.APP,'eboot.bin')
    if d.sha(binary)!=identity['eboot_sha256']:raise RuntimeError('SELF changed before cleanup')
    f.cwd(d.DATA)
    entries=f.nlst()
    if 'perf-test.csv' not in entries: raise RuntimeError('Expected CSV absent')
    for name in ['perf-test.csv','perf-io.csv','perf-consumer.csv','perf-test.log']:
        if name not in entries: continue
        content=d.read(f,d.DATA,name)
        target=out/name
        if target.exists() and target.read_bytes()!=content:raise RuntimeError('Local/remote capture differ: '+name)
        if name=='perf-test.csv' and d.sha(content)!=identity['csv_sha256']:raise RuntimeError('Finite CSV changed')
        if name=='perf-io.csv' and d.sha(content)!=identity['io_csv_sha256']:raise RuntimeError('I/O CSV changed')
        target.write_bytes(content)
        r['files'][name]={'sha256':d.sha(content),'bytes':len(content)}
    for name,info in r['files'].items():
        if d.sha(d.read(f,d.DATA,name))!=info['sha256']:raise RuntimeError('Capture changed before deletion')
        f.cwd(d.DATA);f.delete(name);r['deleted'].append(name)
    f.cwd(d.DATA);r['data_after']=f.nlst()
finally:
    f.quit()
    (out/'cleanup-identity.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r,indent=2),flush=True)
