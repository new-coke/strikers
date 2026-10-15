#!/usr/bin/env python3
"""One INI-selected live-play snapshot, preserving exact installed identities."""
from pathlib import Path
from ftplib import error_perm
from io import BytesIO
import datetime
import hashlib
import json
import sys
import time

ROOT=Path(__file__).resolve().parents[3]
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'smstrikers-port/tools'))
import vita_workflow_device as device

def sha(data): return hashlib.sha256(data).hexdigest()

def main():
    host='192.168.1.79'
    original=(OUT/'strikers.ini.before').read_bytes()
    candidate=(ROOT/'smstrikers-port/build-vita-gxm-latest/strikers_vita.self').read_bytes()
    settings=device.ini(original,{'vita_snapshot_play_frame':'120','vita_snapshot_match_frame':'0',
                                  'gxm_native_visual_debug':'1'})
    shot='debug_frame_play_120.ppm'
    record={'date':datetime.datetime.now().astimezone().isoformat(),
            'self_sha256':sha(candidate),'normal_ini_sha256':sha(original),
            'capture_ini_sha256':sha(settings),'snapshot':shot,'fps_measurement':False}
    ftp=device.connect(host)
    changed=False
    try:
        assert device.read(ftp,device.APP,'eboot.bin')==candidate
        assert device.read(ftp,device.DATA,'strikers.ini')==original
        try:
            old=device.read(ftp,device.DATA,shot)
            (OUT/(shot+'.before')).write_bytes(old)
            ftp.cwd(device.DATA);ftp.delete(shot)
        except error_perm as e:
            if not str(e).startswith('550'):raise
        record['kill_response']=device.command(host,'kill SMSVITA01')
        ftp.cwd(device.DATA)
        changed=True
        ftp.storbinary('STOR strikers.ini',BytesIO(settings))
        assert device.read(ftp,device.DATA,'strikers.ini')==settings
        record['launch_response']=device.command(host,'launch SMSVITA01')
        print(record['launch_response'],flush=True)
        deadline=time.monotonic()+180
        while time.monotonic()<deadline:
            try:
                data=device.read(ftp,device.DATA,shot)
                if data.startswith(b'P6\n960 544\n255\n') and len(data)==len(b'P6\n960 544\n255\n')+960*544*3:
                    (OUT/shot).write_bytes(data)
                    record['snapshot_sha256']=sha(data)
                    print('complete presented-frame snapshot downloaded',flush=True)
                    break
            except error_perm as e:
                if not str(e).startswith('550'):raise
            time.sleep(4)
        else:raise RuntimeError('native play-frame snapshot not completed within 180 seconds')
        status=device.read(ftp,device.DATA,'gxm_native_match_status.csv')
        (OUT/'native-status.visual-fixed.csv').write_bytes(status)
        record['runtime_status']=status.decode(errors='replace')
        coverage=device.read(ftp,device.DATA,'gxm_native_visual_coverage.csv')
        (OUT/'native-visual-coverage.csv').write_bytes(coverage)
        record['coverage_sha256']=sha(coverage)
        assert device.read(ftp,device.APP,'eboot.bin')==candidate
    except Exception as e:
        record['error']=repr(e)
        raise
    finally:
        if changed:
            ftp.cwd(device.DATA)
            ftp.storbinary('STOR strikers.ini',BytesIO(original))
            assert device.read(ftp,device.DATA,'strikers.ini')==original
            record['ini_restored_verified']=True
        (OUT/'visual-capture-identity.json').write_text(json.dumps(record,indent=2)+'\n')
        ftp.quit()

if __name__=='__main__':main()
