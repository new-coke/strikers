#!/usr/bin/env python3
"""Prepare immutable UV0 float3 streams aligned with a checked native mesh pack."""
import argparse
import json
import math
from pathlib import Path
import struct
from asset_pipeline.psarc import parse_psarc,iter_entry
from build_direct_gxm_scene import xxh64,scene_hash,sha256_file

HEADER=struct.Struct('<8sIIQQ')
RECORD=struct.Struct('<II3Q')
MAGIC=b'SMNVUV01'

def uv_stream(data):
    if len(data)<28:return None
    magic,version,kind,sourceBytes,payloadBytes,lo,hi=struct.unpack_from('<7I',data)
    if (magic,version,kind)!=(0x524e5641,2,3) or 28+sourceBytes+payloadBytes!=len(data):return None
    b=memoryview(data)[28+sourceBytes:]
    if len(b)<12 or xxh64(b.tobytes())!=lo|(hi<<32):return None
    vertices,indices,variants=struct.unpack_from('<III',b)
    if not 3<=vertices<=64000 or not 3<=indices<=192000 or indices%3 or not 1<=variants<=64:return None
    offset=12+indices*2
    if offset>len(b):return None
    selected=None
    for _ in range(variants):
        if offset+4>len(b):return None
        count=struct.unpack_from('<I',b,offset)[0];offset+=4
        if not 1<=count<=16 or offset+count*24+4>len(b):return None
        layout=tuple(struct.unpack_from('<6I',b,offset+24*n) for n in range(count));offset+=count*24
        size=struct.unpack_from('<I',b,offset)[0];offset+=4
        if size>len(b)-offset:return None
        expected=((0,4,0,0,32,0),(1,4,2,1,32,16),(3,3,0,0,32,20))
        if layout==expected and size==vertices*32:
            selected=b''.join(b[offset+32*n+20:offset+32*n+32] for n in range(vertices))
        offset+=size
    if offset!=len(b):return None
    if selected is not None and not all(math.isfinite(x[0]) for x in struct.iter_unpack('<f',selected)):return None
    return vertices,selected

def build(archive_path,mesh_path,output):
    mesh=mesh_path.read_bytes();meta=json.loads(mesh_path.with_suffix(mesh_path.suffix+'.json').read_text())
    if len(mesh)<24 or mesh[:8]!=b'SMNVM001' or sha256_file(mesh_path)!=meta['sha256']:raise ValueError('mesh pack identity mismatch')
    version,count,meshHash=struct.unpack_from('<IIQ',mesh,8)
    if version!=1 or count!=len(meta['records']) or count>5000 or scene_hash(mesh[24:])!=meshHash:raise ValueError('invalid mesh pack')
    archive=parse_psarc(archive_path);entry={name:i for i,name in enumerate(archive.paths,start=1)}
    payload=bytearray();offset=24;present=0;vertexCount=0
    with archive_path.open('rb') as f:
        for source in meta['records']:
            fields=struct.unpack_from('<7I3Q',mesh,offset);nv,ni=fields[5:7];offset+=52+nv*20+ni*2
            data=b''.join(iter_entry(f,archive,entry[source['record']]))
            parsed=uv_stream(data)
            if parsed is None or parsed[0]!=nv:raise ValueError('UV layout/topology record mismatch: '+source['record'])
            uv=parsed[1];flags=int(uv is not None);present+=flags;vertexCount+=nv
            payload.extend(RECORD.pack(nv,flags,*fields[7:]))
            # UV-less meshes have defined storage, but the flag prevents a
            # material from inventing coordinates by sampling this default.
            payload.extend(uv if uv is not None else struct.pack('<3f',0.,0.,1.)*nv)
    if offset!=len(mesh):raise ValueError('trailing mesh bytes')
    wire=HEADER.pack(MAGIC,1,count,scene_hash(payload),meshHash)+payload
    if len(wire)>8*1024*1024:raise ValueError('UV stream exceeds 8 MiB cap')
    output.write_bytes(wire)
    report={'kind':'native_uv0_stream','meshes':count,'meshes_with_uv0':present,'vertices':vertexCount,
            'bytes':len(wire),'sha256':sha256_file(output),'mesh_pack_sha256':sha256_file(mesh_path),
            'source_archive_sha256':sha256_file(archive_path)}
    output.with_suffix(output.suffix+'.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--archive',required=True,type=Path)
    p.add_argument('--mesh-pack',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();print(json.dumps(build(a.archive,a.mesh_pack,a.output)))
