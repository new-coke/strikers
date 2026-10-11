#!/usr/bin/env python3
"""Launch one artifact-bound INI profile; all backups remain on the PC."""
from pathlib import Path
import json, sys, time, traceback
BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[1]
sys.path.insert(0, str(ROOT / 'smstrikers-port/tools'))
import vita_workflow_device as d
profile, label = sys.argv[1:3]
plan = json.loads((BASE/'hardware-plan.json').read_text())
config = plan[profile]
out = BASE / label
out.mkdir(exist_ok=True)
if (out/'run-identity.json').exists(): raise SystemExit('Use a new local run label')
r = {'host':'192.168.1.79','time_unix':time.time(),'label':label,'profile':profile,'config':config}
f = None
try:
    r['destroy'] = d.command(r['host'], 'destroy')
    f = d.connect(r['host'])
    binary = d.read(f, d.APP, 'eboot.bin')
    r['eboot_sha256'] = d.sha(binary)
    if r['eboot_sha256'] != plan['candidate_self_sha256']: raise RuntimeError('Unexpected SELF')
    f.cwd('ux0:/data')
    r['coredumps_before'] = sorted(x for x in f.nlst() if 'psp2dmp' in x)
    f.cwd(d.DATA)
    r['data_before'] = f.nlst()
    for name in ['perf-test.csv','perf-io.csv','perf-consumer.csv','perf-test.log']:
        if name in r['data_before']: raise RuntimeError('Archive/verify/remove previous capture first: '+name)
    r['ini'] = d.promote(f, d.DATA, 'strikers.ini', d.ini((BASE/'strikers.ini').read_bytes(), config), out)
    (out/'run-identity.json').write_text(json.dumps(r,indent=2)+'\n')
    r['launch'] = d.command(r['host'],'launch SMSVITA01')
    time.sleep(2)
    r['press'] = d.command(r['host'],'press cross')
    time.sleep(.2)
    r['release'] = d.command(r['host'],'release cross')
    time.sleep(3)
    r['press_retry'] = d.command(r['host'],'press cross')
    time.sleep(.2)
    r['release_retry'] = d.command(r['host'],'release cross')
except Exception as e:
    r['error'] = repr(e)
    r['traceback'] = traceback.format_exc()
finally:
    if f:
        try: f.quit()
        except Exception: f.close()
    (out/'run-identity.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r,indent=2),flush=True)
if 'error' in r: raise SystemExit(1)
