import struct
import unittest
from build_direct_gxm_scene import extract_gpu_color, xxh64, GPU_COLOR_LAYOUT

def record(vertices=3, indices=(0,1,2), bad_position=False):
    data=b''.join(struct.pack('<4f4B', float(i), float(i%2),
                             float('nan') if bad_position else 0.,1.,255,90,40,255)
                  for i in range(vertices))
    payload=struct.pack('<III',vertices,len(indices),1)
    payload+=struct.pack('<'+'H'*len(indices),*indices)
    payload+=struct.pack('<I',len(GPU_COLOR_LAYOUT))
    payload+=b''.join(struct.pack('<6I',*a) for a in GPU_COLOR_LAYOUT)
    payload+=struct.pack('<I',len(data))+data
    checksum=xxh64(payload)
    return struct.pack('<7I',0x524e5641,2,3,1,len(payload),checksum&0xffffffff,checksum>>32)+b'x'+payload

class NativeMeshAdmissionTest(unittest.TestCase):
    def test_real_triangles_and_small_quads_are_not_probe_filtered(self):
        triangle=extract_gpu_color(record())
        self.assertIsNotNone(triangle)
        self.assertEqual((triangle['vertices'],triangle['indices']),(3,3))
        quad=extract_gpu_color(record(4,(0,1,2,0,2,3)))
        self.assertIsNotNone(quad)
        self.assertEqual((quad['vertices'],quad['indices']),(4,6))
        self.assertIsNotNone(extract_gpu_color(record(27,tuple(range(27)))))
    def test_probe_and_match_upper_budgets_are_distinct(self):
        large=record(3001)
        self.assertIsNone(extract_gpu_color(large))
        self.assertIsNotNone(extract_gpu_color(large,max_vertices=64000,max_indices=192000))
    def test_relaxed_small_meshes_still_require_valid_gpu_bytes(self):
        self.assertIsNone(extract_gpu_color(record(3,(0,1,3))))
        self.assertIsNone(extract_gpu_color(record(3,(0,1))))
        self.assertIsNone(extract_gpu_color(record(3,bad_position=True)))
        data=record()
        self.assertIsNone(extract_gpu_color(data[:-1]))
        corrupt=bytearray(data);corrupt[-1]^=1
        with self.assertRaises(ValueError):extract_gpu_color(bytes(corrupt))

if __name__=='__main__':unittest.main()
