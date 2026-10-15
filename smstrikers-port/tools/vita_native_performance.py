#!/usr/bin/env python3
"""Bounded native renderer captures with explicit identity and INI restoration.

Uses only Strikers Companion commands. No binary backup is copied to evidence;
the one rollback SELF lives in the shared build directory.
"""
import argparse
from datetime import datetime
from ftplib import error_perm
from io import BytesIO
from pathlib import Path
import hashlib
import json
import time
import zipfile
import vita_workflow_device as device
from analyze_vita_frames import analyze

ROOT=Path(__file__).resolve().parents[2]
BUILD=ROOT/'smstrikers-port/build-vita-gxm-latest'
BASE=ROOT/'report_performance/evidence/NATIVE_GXM_RENDERER_2026-10-13/native-dynamic-20261013/baseline/strikers.ini'
NORMAL_SHA='7db19fd763528598cc11757a2f1b7918b324f8cf21e0f00d2c2e2440d8d1164f'
HOST='192.168.1.79'
def sha(b):return hashlib.sha256(b).hexdigest()
def stamp():return datetime.now().astimezone().isoformat()
def dump(path,value):path.write_text(json.dumps(value,indent=2)+'\n')
def stopped(journal):
    journal['kill_response']=device.command(HOST,'kill SMSVITA01')
    if journal['kill_response']!='Killed.':
        journal['activate_for_stop']=device.command(HOST,'launch SMSVITA01')
        assert journal['activate_for_stop']=='Launched.'
        time.sleep(3)
        journal['kill_retry']=device.command(HOST,'kill SMSVITA01')
        if journal['kill_retry']!='Killed.':
            raise RuntimeError('Targeted stop unconfirmed; no global kill/reboot: '+journal['kill_retry'])
def write_config(ftp,data):
    ftp.cwd(device.DATA);ftp.storbinary('STOR strikers.ini',BytesIO(data))
    assert device.read(ftp,device.DATA,'strikers.ini')==data

def install(out):
    new=(BUILD/'strikers_vita.self').read_bytes()
    old=(BUILD/'strikers_vita.previous.self').read_bytes()
    with zipfile.ZipFile(BUILD/'strikers_vita.vpk') as archive:
        assert archive.testzip() is None and archive.read('eboot.bin')==new
    j={'started':stamp(),'candidate_sha256':sha(new),'control_sha256':sha(old)}
    f=device.connect(HOST);stage='eboot.bin.native-perf-stage';backup='eboot.bin.native-perf-previous'
    promoted=False
    try:
        ini=device.read(f,device.DATA,'strikers.ini');(out/'strikers.ini.before').write_bytes(ini)
        assert device.read(f,device.APP,'eboot.bin')==old
        f.cwd(device.APP);assert stage not in f.nlst() and backup not in f.nlst()
        stopped(j)
        # Companion can change the app filesystem state while an FTP session
        # is open. Reconnect after the confirmed targeted stop before staging.
        f.close();time.sleep(2);f=device.connect(HOST)
        f.cwd(device.APP);f.storbinary('STOR '+stage,BytesIO(new))
        assert device.read(f,device.APP,stage)==new
        f.cwd(device.APP);f.rename('eboot.bin',backup);promoted=True;f.rename(stage,'eboot.bin')
        assert device.read(f,device.APP,'eboot.bin')==new
        normal=BASE.read_bytes();assert sha(normal)==NORMAL_SHA
        write_config(f,normal)
        j['installed_verified']=True;j['normal_ini_sha256']=sha(normal)
        f.cwd(device.APP);f.delete(backup)
    except Exception as e:
        j['error']=repr(e)
        if promoted:
            f.cwd(device.APP)
            if backup in f.nlst():
                if 'eboot.bin' in f.nlst():f.delete('eboot.bin')
                f.rename(backup,'eboot.bin')
                assert device.read(f,device.APP,'eboot.bin')==old
                j['rollback_verified']=True
        raise
    finally:
        dump(out/'install-identity.json',j);f.close()
    print(json.dumps(j),flush=True)

def run(out,label,slot,timing,frames,skip,compact=0,snapshot=0):
    normal=BASE.read_bytes();assert sha(normal)==NORMAL_SHA
    expected=(BUILD/'strikers_vita.self').read_bytes()
    overrides={'gxm_disable':'0x8','gxm_shader_profile':'WARM','diagnostics':'0',
        'fps_overlay':'0','vita_test_match':'1','vita_frameskip':'0','seed':'0x53545249',
        'fixed_dt':'16.666666667','frame_capture':'ux0:data/strikersVita/'+label+'.csv',
        'frame_capture_frames':str(frames),'frame_capture_match_only':'1','frame_capture_play_only':'1',
        'frame_capture_skip':str(skip),'vita_snapshot_match_frame':'0','vita_snapshot_play_frame':str(snapshot),
        'gxm_native_visual_debug':'0','vita_debug_menu':'0','benchmark':'0','task_profile':'0',
        'gxm_native_slot_fence':str(slot),'gxm_native_timing':str(timing),
        'gxm_native_compact_vertices':str(compact)}
    settings=device.ini(normal,overrides)
    j={'started':stamp(),'label':label,'self_sha256':sha(expected),'ini_sha256':sha(settings),
       'normal_ini_sha256':sha(normal),'config':overrides,'quiet':not timing and not snapshot,'statuses':[]}
    f=device.connect(HOST);changed=False;launched=False
    try:
        assert device.read(f,device.APP,'eboot.bin')==expected
        prior=device.read(f,device.DATA,'strikers.ini');(out/'strikers.ini.before').write_bytes(prior)
        (out/'strikers.ini').write_bytes(settings)
        # A dormant app can report an error to kill. The prior capture ends
        # with a verified stop, so the next launch need not kill it twice.
        if prior!=normal:stopped(j)
        f.cwd(device.DATA)
        assert label+'.csv' not in f.nlst(), 'Capture label already exists'
        shot='debug_frame_play_'+str(snapshot)+'.ppm'
        if snapshot and shot in f.nlst():
            j['prior_snapshot_sha256']=sha(device.read(f,device.DATA,shot))
            f.cwd(device.DATA);f.delete(shot)
        write_config(f,settings);changed=True
        j['launch_response']=device.command(HOST,'launch SMSVITA01');launched=True
        assert j['launch_response']=='Launched.'
        dump(out/'run-identity.json',j)
        print('capture started '+label+' slot='+str(slot)+' timing='+str(timing),flush=True)
        deadline=time.monotonic()+480;last_print=0;last_status=b''
        while time.monotonic()<deadline:
            try:
                data=device.read(f,device.DATA,label+'.csv')
                path=out/(label+'.csv');path.write_bytes(data)
                a=analyze(path,frames,True)
                assert '# strikers-frame-capture-v1 diagnostics=0 fps_overlay=0' in a['metadata']
                dump(out/'analysis.json',a);j['analysis']=a
                break
            except error_perm as e:
                if not str(e).startswith('550'):raise
            except ValueError:
                # A completed capture is a single write; allow its writer to
                # finish before requiring the exact row count.
                pass
            status=device.read(f,device.DATA,'gxm_native_match_status.csv')
            if status!=last_status:
                last_status=status;j['statuses'].append({'time':stamp(),'text':status.decode()})
            if time.monotonic()-last_print>30:
                print(label+' pending; '+status.decode().splitlines()[-1].split(',')[0],flush=True);last_print=time.monotonic()
                dump(out/'run-identity.json',j)
            time.sleep(5)
        else:raise RuntimeError('Capture not complete within 480 seconds')
        final=device.read(f,device.DATA,'gxm_native_match_status.csv');(out/'native-status.csv').write_bytes(final)
        if snapshot:
            pixels=device.read(f,device.DATA,shot)
            assert pixels.startswith(b'P6\n960 544\n255\n') and len(pixels)==1566735
            (out/shot).write_bytes(pixels);j['snapshot_sha256']=sha(pixels)
        j['statuses'].append({'time':stamp(),'text':final.decode()})
        assert 'native_error=0x00000000' in final.decode()
        assert ('dynamic_slot_fence='+str(slot)+' timing='+str(timing)) in final.decode()
        assert ('compact_vertices='+str(compact)) in final.decode()
        assert device.read(f,device.APP,'eboot.bin')==expected
        j['capture_complete']=True
        print(label+' complete '+json.dumps(a),flush=True)
    except Exception as e:
        j['error']=repr(e);raise
    finally:
        if changed:
            write_config(f,normal);j['ini_restored_verified']=True
        if launched:
            j['final_kill_response']=device.command(HOST,'kill SMSVITA01')
            j['process_stopped']=j['final_kill_response']=='Killed.'
        j['finished']=stamp();dump(out/'run-identity.json',j);f.close()
        if launched and not j.get('process_stopped'):
            raise RuntimeError('Configuration restored but process stop was not confirmed')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['install','run']);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--label');p.add_argument('--slot',type=int,choices=[0,1],default=0);p.add_argument('--timing',type=int,choices=[0,1],default=0)
    p.add_argument('--compact',type=int,choices=[0,1],default=0);p.add_argument('--snapshot',type=int,default=0)
    p.add_argument('--frames',type=int,default=300);p.add_argument('--skip',type=int,default=600);a=p.parse_args()
    a.out.mkdir(parents=True,exist_ok=True)
    if a.action=='install':install(a.out)
    else:
        assert a.label and a.label.replace('_','').isalnum() and 1<=a.frames<=8192 and 0<=a.skip<=100000
        assert 0<=a.snapshot<=100000
        run(a.out,a.label,a.slot,a.timing,a.frames,a.skip,a.compact,a.snapshot)
if __name__=='__main__':main()
