#!/usr/bin/env python3
"""Deliver and validate the standalone Strikers renderer with exact identity.

Only targeted Companion commands, atomic SELF delivery, completed captures,
configuration readback and presented-buffer screenshots are used.
"""
import argparse,hashlib,json,time,zipfile
from ftplib import error_perm
from io import BytesIO
from pathlib import Path
import vita_workflow_device as device
from analyze_vita_frames import analyze
ROOT=Path(__file__).resolve().parents[2]
BUILD=ROOT/'smstrikers-port/build-vita-gxm-latest'
NORMAL=ROOT/'report_performance/evidence/VITAGL_2026-10-14/strikers.ini.normal'
HOST='192.168.1.79'
def sha(b):return hashlib.sha256(b).hexdigest()
def save(path,data):path.write_text(json.dumps(data,indent=2)+'\n')
def stop(j,activate=True):
 j['kill']=device.command(HOST,'kill SMSVITA01')
 if j['kill']!='Killed.' and activate:
  j['activate']=device.command(HOST,'launch SMSVITA01');assert j['activate']=='Launched.';time.sleep(3)
  j['kill_retry']=device.command(HOST,'kill SMSVITA01');assert j['kill_retry']=='Killed.'
def config(f,b):
 f.cwd(device.DATA);f.storbinary('STOR strikers.ini',BytesIO(b));assert device.read(f,device.DATA,'strikers.ini')==b

def install(out):
 candidate=(BUILD/'strikers_vita.self').read_bytes()
 with zipfile.ZipFile(BUILD/'strikers_vita.vpk') as archive:assert archive.read('eboot.bin')==candidate
 j={'self_sha256':sha(candidate),'time':time.time()};f=device.connect(HOST)
 old=device.read(f,device.APP,'eboot.bin');j['replaced_sha256']=sha(old)
 stop(j,activate=False);f.close();time.sleep(2);f=device.connect(HOST);stage='eboot.bin.vitagl-stage';backup='eboot.bin.vitagl-previous'
 f.cwd(device.APP);assert stage not in f.nlst()
 rollback=old
 if backup in f.nlst():rollback=device.read(f,device.APP,backup)
 j['rollback_sha256']=sha(rollback)
 f.cwd(device.APP);f.storbinary('STOR '+stage,BytesIO(candidate));assert device.read(f,device.APP,stage)==candidate
 f.cwd(device.APP)
 if backup in f.nlst():f.delete('eboot.bin')
 else:f.rename('eboot.bin',backup)
 try:
  f.rename(stage,'eboot.bin');assert device.read(f,device.APP,'eboot.bin')==candidate
  config(f,NORMAL.read_bytes());j['verified']=True
 except Exception:
  f.cwd(device.APP);f.storbinary('STOR eboot.bin',BytesIO(rollback))
  assert device.read(f,device.APP,'eboot.bin')==rollback
  raise
 else:j['retained_remote_rollback']=backup
 finally:save(out/'install-identity.json',j);f.close()

 print(json.dumps(j),flush=True)

def run(out,label,frames,skip,snapshot,diagnostics,timing,snapshot_phase="play",extra_settings=None,confirm_livearea=False,visual_only=False):
 candidate=(BUILD/'strikers_vita.self').read_bytes();normal=NORMAL.read_bytes()
 overrides={'diagnostics':str(diagnostics),'fps_overlay':'0','benchmark':'0','task_profile':'0','asset_io_profile':'0','vita_view_draw_capture':'0','vita_test_match':'1','vita_frameskip':'0','fixed_dt':'0','seed':'0x53545249','frame_capture':'ux0:data/strikersVita/'+label+'.csv','frame_capture_frames':str(frames),'frame_capture_skip':str(skip),'frame_capture_match_only':'1','frame_capture_play_only':'1','vita_snapshot_play_frame':str(snapshot if snapshot_phase=='play' else 0),'vita_snapshot_match_frame':str(snapshot if snapshot_phase=='match' else 0),'log':'ux0:data/strikersVita/'+label+'.log','vitagl_timing':str(timing)}
 if extra_settings:overrides.update(extra_settings)
 if visual_only:
  assert snapshot>0
  overrides['frame_capture_frames']='0'
 snapshot_name='debug_frame_'+('play' if snapshot_phase=='play' else '3d')+'_'+str(snapshot)+'.ppm'
 settings=device.ini(normal,overrides);j={'self_sha256':sha(candidate),'ini_sha256':sha(settings),'normal_ini_sha256':sha(normal),'config':overrides,'start':time.time(),'statuses':[],'visual_only':visual_only};f=device.connect(HOST);launched=False;changed=False
 try:
  assert device.read(f,device.APP,'eboot.bin')==candidate
  f.cwd(device.DATA)
  assert label+'.csv' not in f.nlst()
  f.cwd('ux0:/data');prior_cores={n for n in f.nlst() if n.endswith('.psp2dmp')};j['prior_cores']=sorted(prior_cores);f.cwd(device.DATA)
  for name in ['vitagl_match_status.csv','native_shader_compile.log','native_rejected_packets.log','native_invalid_matrices.log','native_matrix_producers.log','native_invalid_objects.log','native_ball_packets.log','native_ball_pose.log','native_particle_trace.log','native_renderer_timing.csv','native_simulation_timing.csv','native_transition_trace.csv',snapshot_name]:
   try:b=device.read(f,device.DATA,name);(out/(name+'.prior')).write_bytes(b);f.cwd(device.DATA);f.delete(name)
   except error_perm:pass
  config(f,settings);changed=True;(out/'strikers.ini').write_bytes(settings)
  j['launch']=device.command(HOST,'launch SMSVITA01');assert j['launch']=='Launched.';launched=True
  # Companion can report launch while the app's LiveArea still awaits Start.
  # Confirm once only when this diagnostic run's unique startup log is absent.
  # Never send repeated game input while waiting for a render capture.
  if diagnostics:
   def started():
    f.cwd(device.DATA);f.voidcmd('TYPE I')
    try:return f.size(label+'.log') is not None
    except error_perm as e:
     if not str(e).startswith('550'):raise
     return False
   time.sleep(3);j['started_before_input']=started()
   if not j['started_before_input']:
    j['press']=device.command(HOST,'press cross');time.sleep(.2)
    j['release']=device.command(HOST,'release cross')
   startup_deadline=time.monotonic()+30
   while not started():
    if time.monotonic()>=startup_deadline:raise RuntimeError('No startup log after one LiveArea confirmation')
    time.sleep(2)
   j['startup_log_confirmed']=True
  elif confirm_livearea:
   # Explicit one-shot LiveArea confirmation for quiet runs, before warm-up.
   time.sleep(3);j['launch_confirm']='once'
   j['press']=device.command(HOST,'press cross');time.sleep(.2)
   j['release']=device.command(HOST,'release cross')
  deadline=time.monotonic()+600;last=b'';last_notice=0
  last_core_check=0
  while time.monotonic()<deadline:
   if time.monotonic()-last_core_check>10:
    f.cwd('ux0:/data');new_cores={n for n in f.nlst() if n.endswith('.psp2dmp')}-prior_cores;last_core_check=time.monotonic()
    if new_cores:
     j['crash_names']=sorted(new_cores);j['manual_recovery_required']=True
     for name in new_cores:(out/name).write_bytes(device.read(f,'ux0:/data',name))
     raise RuntimeError('New application crash: '+','.join(sorted(new_cores)))
    f.cwd(device.DATA)
   try:
    if visual_only:
     b=device.read(f,device.DATA,snapshot_name)
     if len(b)==1566735:(out/snapshot_name).write_bytes(b);break
    else:
     b=device.read(f,device.DATA,label+'.csv');path=out/(label+'.csv');path.write_bytes(b);j['analysis']=analyze(path,frames,True);break
   except error_perm:pass
   except ValueError:pass
   try:
    status=device.read(f,device.DATA,'vitagl_match_status.csv')
    if status!=last:j['statuses'].append({'time':time.time(),'text':status.decode()});last=status
    if status and status.decode().splitlines()[-1].split(',')[10]!='0':raise RuntimeError('VitaGL error: '+status.decode())
   except error_perm:pass
   if time.monotonic()-last_notice>30:
    print(label+' pending '+(last.decode().splitlines()[-1] if last else 'native initialization'),flush=True);last_notice=time.monotonic();save(out/'run-identity.json',j)
   time.sleep(3)
  else:raise RuntimeError('No completed capture within 600 seconds')
  for name in ['vitagl_match_status.csv','native_rejected_packets.log','native_invalid_matrices.log','native_matrix_producers.log','native_invalid_objects.log','native_ball_packets.log','native_ball_pose.log','native_particle_trace.log','native_renderer_timing.csv','native_simulation_timing.csv','native_transition_trace.csv','native_shader_compile.log',label+'.log',snapshot_name]:
   try:(out/name).write_bytes(device.read(f,device.DATA,name))
   except error_perm:pass
  if snapshot:assert (out/(snapshot_name)).stat().st_size==1566735
  assert device.read(f,device.APP,'eboot.bin')==candidate
  j['complete']=True;print(label+' complete '+json.dumps(j.get('analysis',{'snapshot':snapshot_name,'visual_only':True})),flush=True)
 except Exception as e:
  j['error']=repr(e)
  # nlBreak abort can create its dump just after the GPU status is published.
  # Detect it before any configuration restore or targeted close operation.
  if launched and not j.get('manual_recovery_required'):
   for attempt in range(4):
    f.cwd('ux0:/data');new_cores={n for n in f.nlst() if n.endswith('.psp2dmp')}-prior_cores
    if new_cores:
     j['crash_names']=sorted(new_cores);j['manual_recovery_required']=True
     for name in new_cores:(out/name).write_bytes(device.read(f,'ux0:/data',name))
     break
    if attempt<3:time.sleep(1)
  raise
 finally:
  for name in ['vitagl_match_status.csv','native_rejected_packets.log','native_invalid_matrices.log','native_matrix_producers.log','native_invalid_objects.log','native_ball_packets.log','native_ball_pose.log','native_particle_trace.log','native_renderer_timing.csv','native_simulation_timing.csv','native_transition_trace.csv','native_shader_compile.log',label+'.log']:
   try:(out/name).write_bytes(device.read(f,device.DATA,name))
   except error_perm:pass
  if not j.get('manual_recovery_required'):
   if changed:config(f,normal);j['restored']=True
   if launched:stop(j,activate=False)
  j['end']=time.time();save(out/'run-identity.json',j);f.close()

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['install','run']);p.add_argument('--out',type=Path,required=True);p.add_argument('--label',default='vitagl_validation');p.add_argument('--frames',type=int,default=240);p.add_argument('--skip',type=int,default=120);p.add_argument('--snapshot',type=int,default=0);p.add_argument('--snapshot-phase',choices=['play','match'],default='play');p.add_argument('--cull-test',type=int,choices=[0,1,2]);p.add_argument('--diagnostics',type=int,choices=[0,1],default=0);p.add_argument('--timing',type=int,choices=[0,1],default=0);p.add_argument('--confirm-livearea',action='store_true',help='confirm Start once before warm-up in a quiet run');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 assert a.label.replace('_','').isalnum() and 1<=a.frames<=8192 and 0<=a.skip<=100000
 if a.action=='install':install(a.out)
 else:run(a.out,a.label,a.frames,a.skip,a.snapshot,a.diagnostics,a.timing,a.snapshot_phase, {"vitagl_cull_test":str(a.cull_test)} if a.cull_test is not None else None,a.confirm_livearea)
if __name__=='__main__':main()
