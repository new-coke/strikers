#!/usr/bin/env python3
"""Read Vita dump thread identities before attributing a crash to this title."""
import gzip
import struct


def thread_names(data):
    if data[:2] == b'\x1f\x8b':
        import io
        with gzip.GzipFile(fileobj=io.BytesIO(data)) as stream:
            data = stream.read(128 * 1024 * 1024 + 1)
    if len(data) > 128 * 1024 * 1024 or len(data) < 52 or data[:6] != b'\x7fELF\x01\x01':
        raise ValueError('Unsupported or oversized Vita dump')
    phoff = struct.unpack_from('<I', data, 28)[0]
    phsize, count = struct.unpack_from('<HH', data, 42)
    if phsize < 32 or count > 4096 or phoff + phsize * count > len(data):
        raise ValueError('Invalid dump program table')
    names = []
    for n in range(count):
        at = phoff + n * phsize
        kind, offset = struct.unpack_from('<II', data, at)
        size = struct.unpack_from('<I', data, at + 16)[0]
        if kind != 4:
            continue
        end = offset + size
        if end > len(data):
            raise ValueError('Truncated dump notes')
        while offset + 12 <= end:
            name_size, desc_size, _ = struct.unpack_from('<III', data, offset)
            offset += 12
            desc = offset + ((name_size + 3) & ~3)
            following = desc + ((desc_size + 3) & ~3)
            if following > end:
                raise ValueError('Truncated dump note')
            name = data[offset:offset + name_size].rstrip(b'\x00')
            if name == b'THREAD_INFO':
                if desc_size < 8:
                    raise ValueError('Truncated thread list')
                threads = struct.unpack_from('<I', data, desc + 4)[0]
                pos = desc + 8
                if threads > 4096:
                    raise ValueError('Invalid thread count')
                for _ in range(threads):
                    if pos + 40 > desc + desc_size:
                        raise ValueError('Truncated thread record')
                    length = struct.unpack_from('<I', data, pos)[0]
                    if length < 40 or pos + length > desc + desc_size:
                        raise ValueError('Invalid thread record')
                    names.append(data[pos + 8:pos + 40].split(b'\x00', 1)[0].decode('ascii', 'replace'))
                    pos += length
            offset = following
    if not names:
        raise ValueError('Dump has no thread identity')
    return names


def classify(data, title='SMSVITA01'):
    names = thread_names(data)
    # Do not infer process identity from eboot.bin, VitaGL or a worker name.
    return {'title': title, 'matches_title': title in names, 'threads': names}
