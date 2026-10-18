"""Offline native graphics compiler input. No retail GLG/GLT payload fallback."""
from __future__ import annotations
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile
from .native import chunks, encoded_size, u16, u32

PACK_MARKER = b'SNPACK01' + struct.pack('<II', 1, 1)

def words_le(data, width=4):
    if len(data)%width: raise ValueError('misaligned numeric payload')
    return b''.join(data[i:i+width][::-1] for i in range(0,len(data),width))

def section(kind, payload):
    return struct.pack('<4I',kind,len(payload),0,0)+payload+bytes((-len(payload))%16)

def container(magic, parts):
    payload=b''.join(parts)
    return struct.pack('<8sII',magic,len(payload)+16,len(parts))+payload

def native_skin(data, start, end, legacy=False):
    parts=[]
    for kind,a,b,raw in chunks(data,start,end):
        payload=data[a:b]
        if kind in (0x1b009,0x1b00f):continue # ignored source metadata
        mapping={0x1b00a:1,0x1b00b:2,0x1b00c:3,0x1b00d:4,0x1b00e:5,0x1b010:6}
        if kind not in mapping:raise ValueError(f'unknown skin section {kind:x}')
        if kind==0x1b00d:
            stride=24 if legacy else 16
            if len(payload)%stride:raise ValueError('skin vertex extent')
            cooked=bytearray()
            for i in range(0,len(payload),stride):
                cooked+=words_le(payload[i:i+12])
                if legacy:cooked+=words_le(payload[i+12:i+24])
                else:cooked+=struct.pack('<3f',*(v/64 for v in struct.unpack_from('3b',payload,i+12)))
                cooked+=bytes(8)
        elif kind==0x1b00e:cooked=words_le(payload,2)
        elif kind==0x1b010:cooked=words_le(payload[:8])+payload[8:]
        else:cooked=words_le(payload)
        parts.append(section(mapping[kind],cooked))
    return container(b'NSSK0001',parts)

def native_model(data):
    first=next(chunks(data,0,len(data)))
    begin,end=(8,first[2]) if first[0]==0x8001b100 else (0,len(data))
    groups=[];statistics={'groups':0,'models':0,'packets':0,'streams':0,'skins':0,'legacy_packet_sizes':[]}
    for kind,a,b,raw in chunks(data,begin,end):
        if kind!=0x8001b000:raise ValueError('unknown model group')
        source=list(chunks(data,raw,b));blocks={k:data[s:e] for k,s,e,r in source}
        required=(0x1b002,0x1b003,0x1b004,0x1b005,0x1b006,0x1b007)
        if any(k not in blocks for k in required):raise ValueError('missing model section')
        models,packets,streams,vertex,indices=(blocks[k] for k in required[1:])
        if not models or len(models)%16 or not streams or len(streams)%6:raise ValueError('model/stream extent')
        packet_count=sum(u32(models,i) for i in range(0,len(models),16))
        if not packet_count or len(packets)%packet_count:raise ValueError('packet count')
        packet_size=len(packets)//packet_count
        if packet_size not in (66,70,74):raise ValueError(f'unknown packet schema {packet_size}')
        statistics['legacy_packet_sizes'].append(packet_size)
        cooked_models=bytearray()
        for i in range(0,len(models),16):
            count,ident,pad,off=struct.unpack_from('>4I',models,i)
            if off%packet_size:raise ValueError('unaligned model packet offset')
            cooked_models+=struct.pack('<4I',count,ident,pad,off//packet_size)
        cooked_packets=bytearray()
        for i in range(packet_count):
            p=packets[i*packet_size:(i+1)*packet_size];out=bytearray(80)
            user,index,count,prim,nstreams,off=struct.unpack_from('>IIHBBI',p)
            if off%6:raise ValueError('unaligned packet stream offset')
            struct.pack_into('<IIHBBI',out,0,user,index,count,prim,nstreams,off//6)
            struct.pack_into('<Q',out,16,struct.unpack_from('>Q',p,16)[0])
            at=24
            material=u32(p,at) if packet_size>=70 else 0
            if packet_size>=70:at+=4
            fields=struct.unpack_from('>9I',p,at);at+=36 # program, raster, matrix, six texture handles
            struct.pack_into('<I9I',out,24,material,*fields)
            out[64:66]=p[at:at+2];at+=2
            state_key=u32(p,at) if packet_size==74 else 0
            if packet_size==74:at+=4
            struct.pack_into('<II',out,68,state_key,u32(p,at))
            cooked_packets+=out
        offsets=sorted({u32(streams,i) for i in range(0,len(streams),6)})
        ends={off:next((x for x in offsets if x>off),len(vertex)) for off in offsets}
        cooked_streams=bytearray();cooked_vertex=bytearray(vertex);converted={}
        for i in range(0,len(streams),6):
            off=u32(streams,i);sid,stride=streams[i+4:i+6];n=ends[off]-off
            if n<=0 or ends[off]>len(vertex) or sid>14 or not stride:raise ValueError('invalid vertex stream')
            if sid==2 or sid==12 or (sid==1 and stride==3):component=1
            elif sid==0 and stride==6 or 3<=sid<=8 and stride==4:component=2
            elif sid in (0,1,9,10,11,13,14) or 3<=sid<=8:component=4
            else:raise ValueError('unknown stream element schema')
            if off in converted and converted[off]!=component:raise ValueError('aliased stream element schemas disagree')
            if off not in converted:
                converted[off]=component
                used=n-n%stride
                if stride%component:raise ValueError('stream stride/component mismatch')
                cooked_vertex[off:off+used]=words_le(vertex[off:off+used],component)
            cooked_streams+=struct.pack('<IBBBBI',off,sid,stride,0,0,n)
        body=[]
        mapping={0x1b002:2,0x1b003:3,0x1b004:4,0x1b005:5,0x1b006:6,0x1b007:7,
                 0x8001b008:8,0x1b00f:9,0x1b011:10,0x1b012:11}
        prepared={0x1b002:words_le(blocks[0x1b002]),0x1b003:bytes(cooked_models),
                  0x1b004:bytes(cooked_packets),0x1b005:bytes(cooked_streams),
                  0x1b006:bytes(cooked_vertex),0x1b007:words_le(indices,2)}
        for k,s,e,r in source:
            if k==0x1b001:continue
            if k not in mapping:raise ValueError(f'unknown model section {k:x}')
            if k in prepared:payload=prepared[k]
            elif k==0x8001b008:payload=native_skin(data,r,e,legacy=packet_size!=74);statistics['skins']+=1
            else:payload=words_le(data[s:e])
            body.append(section(mapping[k],payload))
        groups.append(section(1,b''.join(body)))
        statistics['groups']+=1;statistics['models']+=len(models)//16
        statistics['packets']+=packet_count;statistics['streams']+=len(streams)//6
    return container(b'NSMD0001',groups),statistics


def texture_request(data, *, header=32):
    legacy=header in (16,20)
    if len(data)<header:raise ValueError('texture header truncated')
    levels,fmt=u32(data,0),u32(data,4)
    w,h=u16(data,12 if legacy else 14),u16(data,14 if legacy else 16)
    count=0 if header==16 else u32(data,16 if legacy else 20);bits=data[8:12];missing=0 if legacy else data[12]
    mapping={0:4,1:5,2:10,3:6,4:1,5:0,6:1,7:3,8:8}
    if fmt not in mapping or not 0<w<=4096 or not 0<h<=4096 or not 1<=levels<=13 or count>32767 or missing>1:
        raise ValueError('invalid texture fields')
    if any(x>8 and x!=255 for x in bits) or (fmt==8)!=(count>0):raise ValueError('invalid texture palette/bits')
    n=encoded_size(w,h,mapping[fmt],levels);end=header+n+count*2
    if end>len(data) or len(data)-end>=32:raise ValueError('texture payload extent')
    return b'NSTREQ01'+struct.pack('<I4s8I',fmt,bits,missing,w,h,levels,mapping[fmt],3 if count else 0,n,count*2)+data[header:end]


def texture_bundle_entries(data):
    if len(data)<16 or data[:4]!=b'PTLG':raise ValueError('invalid texture bundle')
    count=u32(data,4)
    candidates=[h for h in (32,16) if h+count*16<=len(data) and
                all(u32(data,h+i*16+4)+u32(data,h+i*16+8)<=len(data)-h-count*16 for i in range(count))]
    if not candidates:raise ValueError('texture bundle dictionary extent')
    h=candidates[0];base=h+count*16;entries=[]
    for i in range(count):
        ident,off,n,gap=struct.unpack_from('>4I',data,h+i*16)
        if gap not in (0,8,12):raise ValueError('unknown texture header layout')
        entries.append((ident,texture_request(data[base+off:base+off+n],header=16 if h==16 and gap==0 else 32-gap)))
    return entries


def native_texture_bundle(entries, compiled):
    at=16+len(entries)*16;dictionary=bytearray();payload=bytearray()
    for ident,request in entries:
        blob=compiled[hashlib.sha256(request).hexdigest()]
        dictionary+=struct.pack('<4I',ident,at,len(blob),0)
        payload+=blob+bytes((-len(blob))%16);at=16+len(entries)*16+len(payload)
    return struct.pack('<8sII',b'NSTB0001',at,len(entries))+dictionary+payload


def resource_header(data,size):
    if len(data)<16:return None
    sector,count,directory,first=struct.unpack_from('>4I',data)
    if sector not in (16,32,64,128,256,512,1024,2048,4096) or count>65536 or not count:return None
    at=directory*sector
    if at<16 or at>size or count*12>size-at or first*sector>size:return None
    return sector,count,at


def resource_bundle(data):
    header=resource_header(data,len(data))
    if header is None:return None
    sector,count,at=header
    entries=[]
    for i in range(count):
        ident,block,n=struct.unpack_from('>3I',data,at+i*12);off=block*sector
        if off>len(data) or n>len(data)-off:return None
        entries.append((ident,data[off:off+n]))
    return sector,entries


def repack_resources(sector,entries):
    first=(sector+len(entries)*12+sector-1)//sector
    out=bytearray(first*sector);struct.pack_into('>4I',out,0,sector,len(entries),1,first)
    for i,(ident,payload) in enumerate(entries):
        at=len(out);struct.pack_into('>3I',out,sector+i*12,ident,at//sector,len(payload))
        out+=payload+bytes((-len(payload))%sector)
    return bytes(out)


def prepare_graphics(roots,compiler,workspace):
    compiler=compiler.resolve()
    if not compiler.is_file():raise ValueError('native graphics compiler is missing')
    workspace=workspace.resolve();models=[];bundles=[];resources=[];requests={};stats=[]
    def admit(request):
        digest=hashlib.sha256(request).hexdigest();requests.setdefault(digest,request);return digest
    for source_root in roots:
        root=source_root.resolve()
        if not root.is_relative_to(workspace):raise ValueError('native graphics must use an extracted private workspace')
        for path in sorted(root.rglob('*')):
            if not path.is_file():continue
            suffix=path.suffix.lower()
            if suffix not in ('.glg','.glt'):
                # Resource dictionaries also use .Dmn and other engine names.
                # Discover by their bounded header, not an extension allowlist.
                with path.open('rb') as probe:header=probe.read(16)
                if resource_header(header,path.stat().st_size) is None:continue
            data=path.read_bytes();original_hash=hashlib.sha256(data).hexdigest()
            if suffix=='.glg':
                try:cooked,detail=native_model(data)
                except ValueError as e:raise ValueError(f'{path.name}: {e}') from e
                models.append((path,cooked));stats.append({'path':path.relative_to(root.parent).as_posix(),'source_sha256':original_hash,'native_sha256':hashlib.sha256(cooked).hexdigest(),**detail})
            elif suffix=='.glt':
                entries=texture_bundle_entries(data)
                for _,request in entries:admit(request)
                bundles.append((path,entries,original_hash))
            else:
                bundle=resource_bundle(data)
                if bundle is None:continue
                sector,entries=bundle;converted=[];hits=0
                for ident,payload in entries:
                    request=None
                    # Resource bundles in the live game use the current 32-byte texture header.
                    try:request=texture_request(payload)
                    except ValueError:pass
                    if request is not None:
                        converted.append((ident,admit(request)));hits+=1
                    else:converted.append((ident,payload))
                if hits:resources.append((path,sector,converted,hits,original_hash))
    with tempfile.TemporaryDirectory(prefix='native-graphics-',dir=workspace) as tmp:
        work=Path(tmp);input_dir=work/'requests';output_dir=work/'compiled';input_dir.mkdir()
        for digest,request in requests.items():(input_dir/(digest+'.txrq')).write_bytes(request)
        result=subprocess.run([str(compiler),str(input_dir),str(output_dir)],capture_output=True,text=True,check=True)
        compiled={digest:(output_dir/(digest+'.nstx')).read_bytes() for digest in requests}
        for path,cooked in models:path.write_bytes(cooked)
        for path,entries,original_hash in bundles:
            payload=native_texture_bundle(entries,compiled);path.write_bytes(payload)
            stats.append({'path':str(path.relative_to(workspace)),'source_sha256':original_hash,'native_sha256':hashlib.sha256(payload).hexdigest(),'textures':len(entries)})
        for path,sector,entries,hits,original_hash in resources:
            payload=repack_resources(sector,[(ident,compiled[value] if isinstance(value,str) else value) for ident,value in entries]);path.write_bytes(payload)
            stats.append({'path':str(path.relative_to(workspace)),'source_sha256':original_hash,'native_sha256':hashlib.sha256(payload).hexdigest(),'embedded_textures':hits})
        # The same bounded parser is used by the Vita loaders.
        for root in roots:subprocess.run([str(compiler),'--validate',str(root)],capture_output=True,text=True,check=True)
    for root in roots:
        if root.name=='sys':(root/'strikers-native.bin').write_bytes(PACK_MARKER)
    return {'abi':'strikers-native-graphics-v1','model_files':len(models),'texture_bundles':len(bundles),
            'bundle_textures':sum(len(e) for _,e,_ in bundles),'resource_bundles':len(resources),
            'embedded_textures':sum(n for _,_,_,n,_ in resources),'unique_textures':len(requests),
            'compiler':result.stdout.strip(),'files':stats,
            'policy':'native model arrays and state; native linear RGBA mip payloads; no original GLG/GLT fallback'}
