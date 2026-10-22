#!/usr/bin/env python3
"""Validate finite producer/owner timeline and Material subsets against packet identity.

Absolute events use one kernel clock. API durations remain owner wall time and may
include scheduling/driver waits; they neither establish GPU execution time nor a
performance gain. The optional CLI also validates the original frame CSV using the
existing analyzer. No files are written until every input has passed validation.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re

IDENTITY = ('sample', 'live_frame', 'engine_frame', 'discarded')
PACKET_PHASES = ('invalidation_ns', 'arrays_ns', 'skin_ns', 'material_ns', 'geometry_ns', 'submit_ns', 'cleanup_ns')
PACKET_COUNTERS = ('attempts', 'accepted', 'rejected', 'resident_hits', 'resident_builds', 'dynamic_draws',
                   'input_references', 'decoded_vertices', 'decoded_indices', 'vertex_bytes', 'index_bytes', 'packet_wait_ns')
PACKET_OWNER = ('owner_draw_us', 'owner_present_us', 'frontend_wait_us', 'command_bytes', 'command_copy_ns',
                'command_vertex_bytes', 'command_index_bytes', 'slot_waits', 'renderer_draws')
PACKET_FIELDS = (*IDENTITY, 'packet_ns', *PACKET_PHASES, *PACKET_COUNTERS, *PACKET_OWNER)
TIMELINE_FIELDS = (*IDENTITY,
 'producer_begin_request_ns','producer_begin_return_ns','first_draw_ready_ns','first_draw_publish_ns','producer_end_request_ns','producer_end_return_ns',
 'producer_batches','draw_batches','producer_draw_commands','borrowed_draw_commands','begin_succeeded','end_succeeded','producer_cancelled',
 'owner_begin_start_ns','owner_begin_end_ns','first_draw_command_ns','last_draw_command_end_ns','owner_end_start_ns','owner_end_end_ns',
 'owner_batches','owner_draw_commands','failed_draw_commands','ready_wait_calls','stored_waits','wait_overflow','ready_wait_ns','max_ready_wait_ns',
 'draw_timing_enabled','draw_prepare_ns','draw_api_ns','draw_suffix_ns','timed_draw_scopes','present_requested','present_prefix_ns','present_swap_ns','present_suffix_ns','timed_present_scopes','owner_cancelled',
 'display_add_entry_ns','display_add_entry_calls','display_add_entry_errors','display_add_entry_last_result','gc_ns','gc_calls','gc_errors','gc_last_result')
MATERIAL_PARTS = ('viewports_matrices_ns','state_lighting_ns','texture_resolution_ns')
MATERIAL_COUNTS = ('material_attempts','viewport_overrides','viewport_matrix_overrides','model_concats','view_only_matrices','invalid_matrices',
                   'point_lights','directional_lights','specular_lights','texture_requests','texture_resolutions','texture_failures')
MATERIAL_FIELDS = (*IDENTITY,*MATERIAL_PARTS,*MATERIAL_COUNTS)
WAIT_FIELDS = (*IDENTITY,'interval','begin_ns','end_ns')
METADATA = {
 '.packets.csv': '# strikers-packet-profile-v1 attribution_only=1 waits_are_subsets=1',
 '.timeline.csv': '# strikers-frame-timeline-v1 clock=system_time_ns owner_waits_are_subsets=1 max_intervals=32',
 '.timeline_waits.csv': '# strikers-frame-timeline-waits-v1 clock=system_time_ns max_intervals=32',
 '.materials.csv': '# strikers-material-profile-v1 children_partition_material=1 counts_from_existing_operations=1'}
BOOLS = ('discarded','begin_succeeded','end_succeeded','producer_cancelled','draw_timing_enabled','present_requested','owner_cancelled')
SIGNED = ('display_add_entry_last_result','gc_last_result')
U32 = ('engine_frame','producer_batches','draw_batches','producer_draw_commands','borrowed_draw_commands','owner_batches','owner_draw_commands',
       'failed_draw_commands','ready_wait_calls','stored_waits','wait_overflow','timed_draw_scopes','timed_present_scopes',
       'display_add_entry_calls','display_add_entry_errors','gc_calls','gc_errors')
LIMITS = [
 'Absolute producer/owner events use sceKernelGetSystemTimeWide, exported in ns at microsecond source resolution. Material children use their existing parent clock.',
 'Draw and present children partition existing owner wall timers. Driver calls may include CPU work, scheduling and GPU/back-pressure waits; no component is pure GPU time.',
 'The GC subset covers the normal swap footer request/ack only. DisplayQueueAddEntry is measured separately; both are subsets of present_swap_ns.',
 'Ready waits start only after the first Draw command. Their aggregate retains all waits; at most 32 raw intervals are stored and overflow remains explicit.',
 'Command counts and timed draw scopes differ: validation/cull exits can have no timed scope; internal copy draws can contribute scopes without a queued Draw command.',
 'Isolated bitmap/loading transactions restore scene timers and are excluded from their children. Other command gaps remain unclassified.',
 'Only finite selected frames are retained. Instrumented wall time includes instrumentation cost; compare a quiet run separately. No FPS or speedup is inferred here.',
 'A missing event is treated as partial/failed/cancelled when its lifecycle permits it; it is never assigned a zero-length successful operation.']

def check(condition, reason):
    if not condition:
        raise ValueError(reason)


def read_sidecar(base, suffix, fields, allow_empty=False):
    path = Path(str(base)+suffix)
    with path.open('rb') as source:
        data = source.read(64*1024*1024+1)
    check(len(data)<=64*1024*1024, 'sidecar exceeds byte bound')
    check(data.endswith(b'\n'), 'incomplete sidecar final line')
    try:
        text=data.decode('ascii')
        lines=text.splitlines()
        check(lines and lines[0]==METADATA[suffix], 'unexpected sidecar metadata: '+suffix)
        reader=csv.DictReader(io.StringIO('\n'.join(lines[1:])), strict=True)
        check(reader.fieldnames==list(fields), 'unexpected sidecar columns: '+suffix)
        rows=[]
        for raw in reader:
            check(set(raw)==set(fields) and all(v is not None for v in raw.values()), 'malformed sidecar row')
            row={}
            for key in fields:
                signed=key in SIGNED
                check(re.fullmatch(r'-?(?:0|[1-9][0-9]*)' if signed else r'(?:0|[1-9][0-9]*)',raw[key]) is not None,
                      'noncanonical integer: '+key)
                value=int(raw[key]);limit=0x7fffffff if signed else 0xffffffffffffffff
                check((-0x80000000 if signed else 0)<=value<=limit, 'integer overflow: '+key)
                row[key]=value
            rows.append(row)
            check(len(rows)<=(8192*32 if suffix=='.timeline_waits.csv' else 8192), 'row bound exceeded')
        check(rows or allow_empty, 'empty sidecar: '+suffix)
    except (UnicodeError,csv.Error) as error:
        raise ValueError('malformed sidecar encoding/CSV') from error
    return rows, {'path':str(path),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}


def same_identity(row, packet):
    check(all(row[key]==packet[key] for key in IDENTITY), 'sidecar/packet identity mismatch')


def summary(values):
    return {'count':len(values),'total':sum(values),'mean':sum(values)/len(values) if values else None,
            'max':max(values) if values else None}


def analyze_sidecars(capture):
    capture=Path(capture)
    packets,pin=read_sidecar(capture,'.packets.csv',PACKET_FIELDS)
    timelines,pin2=read_sidecar(capture,'.timeline.csv',TIMELINE_FIELDS)
    materials,pin3=read_sidecar(capture,'.materials.csv',MATERIAL_FIELDS)
    waits,pin4=read_sidecar(capture,'.timeline_waits.csv',WAIT_FIELDS,True)
    check(len(packets)==len(timelines)==len(materials), 'sample counts differ')
    n=len(packets)
    raw_waits=[[] for _ in packets]
    prior_sample=-1
    for wait in waits:
        sample=wait['sample']
        check(sample<n and sample>=prior_sample, 'unknown/out-of-order wait sample')
        same_identity(wait,packets[sample]);raw_waits[sample].append(wait);prior_sample=sample
    frames=[];previous_end=0;mode=timelines[0]['draw_timing_enabled']
    for i,(packet,row,material) in enumerate(zip(packets,timelines,materials)):
        check(packet['sample']==i and packet['discarded'] in (0,1), 'packet identity/order invalid')
        check(packet['packet_ns']==sum(packet[x] for x in PACKET_PHASES), 'packet phase sum mismatch')
        check(packet['accepted']+packet['rejected']==packet['attempts'], 'packet acceptance sum mismatch')
        same_identity(row,packet);same_identity(material,packet)
        check(all(row[key] in (0,1) for key in BOOLS), 'invalid timeline boolean')
        check(all(row[key]<=0xffffffff for key in U32), 'uint32 counter overflow')
        check(row['draw_timing_enabled']==mode, 'timing mode changes within capture')
        a=material['material_attempts']
        check(sum(material[x] for x in MATERIAL_PARTS)==packet['material_ns'], 'Material children do not partition parent')
        check(a<=packet['attempts'], 'Material attempts exceed packets')
        check(material['viewport_matrix_overrides']<=material['viewport_overrides']<=a, 'viewport counts inconsistent')
        check(material['model_concats']+material['view_only_matrices']<=a, 'matrix counts inconsistent')
        check(material['invalid_matrices']<=material['model_concats']+material['view_only_matrices'], 'invalid-matrix count inconsistent')
        check(all(material[x]<=4*a for x in ('point_lights','directional_lights','specular_lights')), 'light count exceeds visited cap')
        check(material['texture_requests']<=6*a and material['texture_failures']<=a and
              material['texture_resolutions']+material['texture_failures']<=material['texture_requests'], 'texture counts inconsistent')
        draw_parts=sum(row[x] for x in ('draw_prepare_ns','draw_api_ns','draw_suffix_ns'))
        present_parts=sum(row[x] for x in ('present_prefix_ns','present_swap_ns','present_suffix_ns'))
        check(draw_parts==packet['owner_draw_us']*1000 and present_parts==packet['owner_present_us']*1000,
              'owner children do not partition parent')
        if not mode:
            check(not draw_parts and not present_parts and not row['timed_draw_scopes'] and not row['timed_present_scopes'], 'OFF owner timing is nonzero')
        if not row['timed_draw_scopes']:check(not draw_parts, 'draw duration without completed scope')
        if not row['timed_present_scopes']:check(not present_parts, 'present duration without completed scope')
        check(row['timed_present_scopes']<=1, 'multiple normal presents in one frame')
        if not row['present_requested']:
            check(not present_parts and not row['timed_present_scopes'], 'unrequested normal present measured')
        check(not packet['discarded'] or not row['present_requested'], 'discarded frame requested scene present')
        for prefix in ('display_add_entry','gc'):
            calls=row[prefix+'_calls'];errors=row[prefix+'_errors'];result=row[prefix+'_last_result']
            check(errors<=calls<=row['timed_present_scopes'], 'provider call/error counts inconsistent')
            if not calls:check(not row[prefix+'_ns'] and not errors and not result, 'provider data without call')
            if result<0:check(errors>0, 'negative provider result without error count')
            if calls==1:check(errors==int(result<0), 'provider result/error mismatch')
        check(row['display_add_entry_ns']+row['gc_ns']<=row['present_swap_ns'], 'provider subsets exceed swap')
        check(row['borrowed_draw_commands']<=row['producer_draw_commands'] and row['draw_batches']<=row['producer_batches'], 'producer quantities inconsistent')
        check(row['owner_draw_commands']==row['producer_draw_commands'] and row['owner_batches']==row['producer_batches'], 'drained producer/owner quantities differ')
        check(row['failed_draw_commands']<=row['owner_draw_commands'], 'failed Draw count exceeds commands')
        begin=row['producer_begin_request_ns'];begin_return=row['producer_begin_return_ns']
        check(begin>0 and begin_return>=begin and begin>=previous_end, 'producer Begin ordering/epoch invalid')
        if row['owner_begin_start_ns']:
            check(begin<=row['owner_begin_start_ns']<=row['owner_begin_end_ns']<=begin_return, 'owner Begin ordering invalid')
        else:check(not row['begin_succeeded'] and not row['owner_begin_end_ns'], 'successful Begin has no owner events')
        if row['producer_end_return_ns']:
            check(row['begin_succeeded'] and begin_return<=row['producer_end_request_ns']<=row['owner_end_start_ns']<=row['owner_end_end_ns']<=row['producer_end_return_ns'], 'End/drain ordering invalid')
            previous_end=row['producer_end_return_ns']
        else:
            check(not row['end_succeeded'] and not row['producer_end_request_ns'] and not row['owner_end_start_ns'] and not row['owner_end_end_ns'], 'partial End events inconsistent')
            check(not row['begin_succeeded'] or row['producer_cancelled'], 'successful Begin has neither End nor cancellation')
            previous_end=begin_return
        check(row['producer_cancelled']==row['owner_cancelled'] or not row['owner_begin_start_ns'], 'cancellation ownership mismatch')
        draws=row['producer_draw_commands']
        if draws:
            check(row['begin_succeeded'] and row['draw_batches'] and begin_return<=row['first_draw_ready_ns']<=row['first_draw_publish_ns']<=row['first_draw_command_ns']<=row['last_draw_command_end_ns'], 'first Draw ordering invalid')
            if row['owner_end_start_ns']:check(row['last_draw_command_end_ns']<=row['owner_end_start_ns'], 'Draw extends past End')
        else:
            check(not any(row[x] for x in ('draw_batches','first_draw_ready_ns','first_draw_publish_ns','first_draw_command_ns','last_draw_command_end_ns','ready_wait_calls')), 'fabricated Draw/wait events')
        count=row['ready_wait_calls'];stored=row['stored_waits'];overflow=row['wait_overflow']
        check(stored==min(count,32) and overflow==count-stored and len(raw_waits[i])==stored, 'wait capacity/overflow count mismatch')
        durations=[];end=0
        for index,wait in enumerate(raw_waits[i]):
            check(wait['interval']==index and wait['begin_ns']>=end and wait['end_ns']>=wait['begin_ns'], 'wait interval ordering invalid')
            check(wait['begin_ns']>=row['first_draw_command_ns'], 'wait precedes first Draw')
            if row['owner_end_start_ns']:check(wait['end_ns']<=row['owner_end_start_ns'], 'wait extends past owner End')
            durations.append(wait['end_ns']-wait['begin_ns']);end=wait['end_ns']
        check(sum(durations)<=row['ready_wait_ns'] and max(durations,default=0)<=row['max_ready_wait_ns']<=row['ready_wait_ns'], 'wait aggregate inconsistent')
        if not overflow:check(sum(durations)==row['ready_wait_ns'] and max(durations,default=0)==row['max_ready_wait_ns'], 'complete wait aggregate mismatch')
        if not count:check(not row['ready_wait_ns'] and not row['max_ready_wait_ns'], 'wait duration without call')
        if row['owner_end_start_ns'] and draws:
            check(row['ready_wait_ns']<=row['owner_end_start_ns']-row['first_draw_command_ns'], 'wait sum exceeds owner span')
        derived={'begin_sync_ns':begin_return-begin,
                 'first_draw_publish_delay_ns':row['first_draw_publish_ns']-row['first_draw_ready_ns'] if draws else None,
                 'first_draw_owner_delay_ns':row['first_draw_command_ns']-row['first_draw_publish_ns'] if draws else None,
                 'end_sync_ns':row['producer_end_return_ns']-row['producer_end_request_ns'] if row['producer_end_return_ns'] else None,
                 'swap_other_ns':row['present_swap_ns']-row['display_add_entry_ns']-row['gc_ns'],
                 'unfinished_texture_requests':material['texture_requests']-material['texture_resolutions']-material['texture_failures']}
        frames.append({'identity':{k:row[k] for k in IDENTITY},'events_and_counts':{k:v for k,v in row.items() if k not in IDENTITY},
                       'derived':derived,'material':{k:material[k] for k in (*MATERIAL_PARTS,*MATERIAL_COUNTS)},
                       'stored_ready_waits':[[w['begin_ns'],w['end_ns']] for w in raw_waits[i]],
                       'complete_lifecycle':bool(row['begin_succeeded'] and row['producer_end_return_ns'] and not row['producer_cancelled'])})
    duration_keys=('draw_prepare_ns','draw_api_ns','draw_suffix_ns','present_prefix_ns','present_swap_ns','present_suffix_ns','display_add_entry_ns','gc_ns','ready_wait_ns')
    return {'schema':'strikers-frame-timeline-analysis-v1','samples':n,'clock':'system_time_ns','timing_enabled':bool(mode),
            'inputs':[pin,pin2,pin3,pin4],
            'owner_ns':{k:summary([r[k] for r in timelines]) for k in duration_keys},
            'material_ns':{k:summary([r[k] for r in materials]) for k in MATERIAL_PARTS},
            'material_counts':{k:summary([r[k] for r in materials]) for k in MATERIAL_COUNTS},
            'derived_ns':{k:summary([f['derived'][k] for f in frames if f['derived'][k] is not None]) for k in ('begin_sync_ns','first_draw_publish_delay_ns','first_draw_owner_delay_ns','end_sync_ns','swap_other_ns')},
            'stored_waits':len(waits),'wait_overflow':sum(r['wait_overflow'] for r in timelines),
            'failed_draw_commands':sum(r['failed_draw_commands'] for r in timelines),
            'provider_errors':sum(r['display_add_entry_errors']+r['gc_errors'] for r in timelines),
            'frames':frames,'limits':LIMITS}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path)
    parser.add_argument('--expected',type=int)
    parser.add_argument('--allow-nonplay',action='store_true')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    # Existing analysis establishes the frame CSV's selection and cadence; the
    # sidecar-only helper above is used by host fixtures, not a gameplay claim.
    from analyze_packet_profile import analyze as analyze_packets
    baseline=analyze_packets(args.capture,args.expected,require_live_play=not args.allow_nonplay)
    result=analyze_sidecars(args.capture)
    result['frame_and_packet_validation']=baseline
    encoded=json.dumps(result,indent=2)+'\n'
    if args.output:
        with args.output.open('x') as output:output.write(encoded)
    else:print(encoded,end='')

if __name__=='__main__':main()
