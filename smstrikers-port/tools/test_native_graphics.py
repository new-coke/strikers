from pathlib import Path
import struct
import unittest
import hashlib
import tempfile
import os
from asset_pipeline.native_graphics import native_model, native_skin, texture_bundle_entries, resource_bundle, repack_resources, prepare_graphics
from vita_native_manifest import manifest


def chunk(kind,payload):return struct.pack('>II',kind,len(payload))+payload

def sections(data,start=16):
    result={};at=start
    while at<len(data):
        kind,n,_,_=struct.unpack_from('<4I',data,at);result[kind]=data[at+16:at+16+n];at+=16+(n+15)//16*16
    return result

class NativeGraphicsTest(unittest.TestCase):
    @unittest.skipUnless(os.environ.get('STRIKERS_GRAPHICS_COMPILER'),'host graphics compiler required')
    def test_resource_images_are_converted_for_dmn_and_unknown_extensions(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace=Path(directory);root=workspace/'files';root.mkdir()
            texture=bytearray(64);struct.pack_into('>II',texture,0,1,2)
            texture[8:12]=bytes([5,6,5,0]);struct.pack_into('>HH',texture,14,8,8)
            for name in ('MainUI.Dmn','future-extension.bundle'):
                (root/name).write_bytes(repack_resources(32,[(17,bytes(texture)),(23,b'unchanged metadata')]))
            report=prepare_graphics([root],Path(os.environ['STRIKERS_GRAPHICS_COMPILER']),workspace)
            self.assertEqual(report['resource_bundles'],2)
            self.assertEqual(report['embedded_textures'],2)
            self.assertEqual(report['unique_textures'],1)
            for path in root.iterdir():
                entries=resource_bundle(path.read_bytes())[1]
                self.assertEqual(entries[0][0],17)
                self.assertEqual(entries[0][1][:8],b'NSTX0001')
                self.assertEqual(entries[1],(23,b'unchanged metadata'))
    def test_delivery_manifest_matches_runtime_wire_contract_and_chunk_boundaries(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'source'
            source.write_bytes(b'abc')
            blob,report=manifest(source)
            self.assertEqual(len(blob),96)
            self.assertEqual(struct.unpack_from('<8sIIQI',blob),(b'NSVERIFY',1,1048576,3,1))
            known=bytes.fromhex('ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
            self.assertEqual(blob[28:60],known)
            self.assertEqual(blob[64:],known)
            self.assertEqual(report['archive_sha256'],known.hex())
            data=b'x'*1048576+b'abc';source.write_bytes(data)
            blob,report=manifest(source)
            self.assertEqual(report['chunks'],2)
            self.assertEqual(blob[64:96],hashlib.sha256(data[:-3]).digest())
            self.assertEqual(blob[96:],known)
            source.write_bytes(b'')
            with self.assertRaises(ValueError):manifest(source)
    def test_all_packet_schemas_keep_materials_indices_and_native_vertex_values(self):
        for size in (66,70,74):
            with self.subTest(size=size):
                packet=bytearray(size);struct.pack_into('>IIHBBI',packet,0,0,0,3,0,1,0)
                struct.pack_into('>Q',packet,16,0x0102030405060708);at=24
                if size>=70:struct.pack_into('>I',packet,at,9);at+=4
                struct.pack_into('>9I',packet,at,0x11223344,0x55667788,0,1,2,3,4,5,6);at+=36
                packet[at:at+2]=bytes([3,0]);at+=2
                if size==74:struct.pack_into('>I',packet,at,17);at+=4
                struct.pack_into('>I',packet,at,23)
                vertices=struct.pack('>9f',1.25,-2.5,.125,0,1,0,0,0,1)
                body=b''.join(chunk(kind,payload) for kind,payload in [
                    (0x1b002,bytes(64)),(0x1b003,struct.pack('>4I',1,99,0,0)),
                    (0x1b004,packet),(0x1b005,struct.pack('>IBB',0,0,12)),
                    (0x1b006,vertices),(0x1b007,struct.pack('>3H',0,1,2))])
                cooked,report=native_model(chunk(0x8001b100,chunk(0x8001b000,body)))
                self.assertEqual(cooked[:8],b'NSMD0001');group=sections(sections(cooked)[1],0)
                out=group[4];self.assertEqual(len(out),80)
                self.assertEqual(struct.unpack_from('<Q',out,16)[0],0x0102030405060708)
                self.assertEqual(struct.unpack_from('<II',out,28),(0x11223344,0x55667788))
                self.assertEqual(struct.unpack_from('<I',out,72)[0],23)
                self.assertEqual(group[7],struct.pack('<3H',0,1,2))
                self.assertEqual(struct.unpack_from('<3f',group[6]),(1.25,-2.5,.125))
                self.assertEqual(group[5][6],0);self.assertEqual(report['packets'],1)
    def test_skin_normals_remain_exact_for_float_and_packed_sources(self):
        for legacy in (False,True):
            position=struct.pack('>3f',.5,-1,2)
            normal=struct.pack('>3f',.123456,-.5,1) if legacy else bytes([32,224,64,0])
            skin=native_skin(chunk(0x1b00d,position+normal),0,8+len(position+normal),legacy=legacy)
            vertex=sections(skin)[4];self.assertEqual(len(vertex),32)
            self.assertEqual(struct.unpack_from('<3f',vertex),(.5,-1,2))
            if legacy:self.assertEqual(vertex[12:24],normal[:4][::-1]+normal[4:8][::-1]+normal[8:12][::-1])
            else:self.assertEqual(struct.unpack_from('<3f',vertex,12),(.5,-.5,1))
    def test_every_texture_header_layout_is_converted_without_guessing(self):
        for header in (16,20,24,32):
            h=32 if header==32 else 16;texture=bytearray(header)
            struct.pack_into('>II',texture,0,1,2);texture[8:12]=bytes([5,6,5,0])
            struct.pack_into('>HH',texture,12 if header<=20 else 14,8,8);texture+=bytes(32)
            gap=0 if header in (16,32) else 32-header
            bundle=bytearray(h);bundle[:4]=b'PTLG';struct.pack_into('>I',bundle,4,1)
            bundle+=struct.pack('>4I',99,0,len(texture),gap)+texture
            entries=texture_bundle_entries(bundle);self.assertEqual(len(entries),1)
            self.assertEqual(struct.unpack_from('<3I',entries[0][1],20),(8,8,1))
            with self.assertRaises(ValueError):texture_bundle_entries(bundle[:-1])
    def test_embedded_resource_repack_preserves_hashes_and_non_texture_payloads(self):
        original=[(1,b'font metadata'),(2,b'native texture payload'*17)]
        data=repack_resources(32,original);self.assertEqual(resource_bundle(data),(32,original))
        self.assertIsNone(resource_bundle(data[:-20]))

if __name__=='__main__':unittest.main()
