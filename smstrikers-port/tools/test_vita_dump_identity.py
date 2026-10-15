import gzip
import struct
import unittest
from vita_dump_identity import classify

def dump(title):
    record=struct.pack('<II32s',40,1,title.encode())
    desc=struct.pack('<II',0,1)+record
    note=struct.pack('<III',12,len(desc),0)+b'THREAD_INFO\0'+desc
    header=bytearray(84);header[:6]=b'\x7fELF\x01\x01'
    struct.pack_into('<I',header,28,52);struct.pack_into('<HH',header,42,32,1)
    struct.pack_into('<II',header,52,4,84);struct.pack_into('<I',header,68,len(note))
    return bytes(header)+note

class Identity(unittest.TestCase):
    def test_title_and_gzip(self):
        for title in ('SMSVITA01','PHGV00001'):
            for blob in (dump(title),gzip.compress(dump(title))):
                self.assertEqual(classify(blob)['matches_title'],title=='SMSVITA01')
    def test_workers_are_not_process_identity(self):
        self.assertFalse(classify(dump('vitaGL Splashscreen'))['matches_title'])
    def test_truncated(self):
        for blob in (b'',b'eboot.bin',dump('SMSVITA01')[:-1]):
            with self.assertRaises(ValueError):classify(blob)

if __name__=='__main__':unittest.main()
