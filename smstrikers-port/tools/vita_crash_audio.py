"""Bounded, offline checks for a reviewed ARM audio NULL-track crash.

No file access or tool execution occurs here. This verifies the retained
instructions, unwind records and captured memory; it does not infer an entire
execution history or extend the manually reviewed seven-frame chain.
"""
import gzip
import hashlib
import io
import re
import struct


PROFILE = 'audio_null_track'
MAX_CORE = 32 * 1024 * 1024
MAX_FUNCTION = 16 * 1024
MAX_CODE = 64 * 1024


class AudioEvidenceError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise AudioEvidenceError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def uint(value, maximum=0xffffffff):
    require(type(value) is int and 0 <= value <= maximum, 'Invalid bounded unsigned value')
    return value


def take(data, offset, count):
    uint(offset)
    uint(count)
    require(offset <= len(data) and count <= len(data) - offset, 'Out-of-range ELF/core slice')
    return data[offset:offset + count]


def u32(data, offset):
    return struct.unpack('<I', take(data, offset, 4))[0]


def raw_hex(value, maximum, *, size=None, digest=None):
    require(isinstance(value, str) and 0 < len(value) <= maximum * 2
            and len(value) % 2 == 0 and re.fullmatch('[0-9a-f]+', value), 'Invalid bounded evidence hex')
    result = bytes.fromhex(value)
    require(size is None or len(result) == uint(size, maximum), 'Evidence hex size mismatch')
    require(digest is None or (isinstance(digest, str) and sha(result) == digest), 'Evidence hex hash mismatch')
    return result


def cstring(data, offset, bound=4096):
    uint(offset)
    require(offset < len(data), 'Invalid string offset')
    end = data.find(b'\0', offset, min(len(data), offset + bound))
    require(end >= 0, 'Unterminated bounded string')
    try:
        return data[offset:end].decode('ascii')
    except UnicodeError as error:
        raise AudioEvidenceError('Non-ASCII ELF/core name') from error


def elf_header(data):
    require(len(data) >= 52 and data[:7] == b'\x7fELF\x01\x01\x01', 'Expected ELF32 little endian')
    require(struct.unpack_from('<H', data, 18)[0] == 40, 'Expected ARM ELF')
    return data[:52]


def program_headers(data):
    elf_header(data)
    offset = u32(data, 28)
    stride, count = struct.unpack('<HH', take(data, 42, 4))
    require(32 <= stride <= 128 and 0 < count < 512, 'Invalid program header count')
    result = []
    for index in range(count):
        raw = take(data, offset + index * stride, 32)
        values = struct.unpack('<8I', raw)
        row = dict(zip(('type', 'offset', 'vaddr', 'paddr', 'filesz', 'memsz', 'flags', 'align'), values))
        take(data, row['offset'], row['filesz'])
        require(row['vaddr'] + row['memsz'] <= 0x100000000, 'Program address overflow')
        row['raw_hex'] = raw.hex()
        result.append(row)
    return result


def sections(data):
    elf_header(data)
    offset = u32(data, 32)
    stride, count, strings_index = struct.unpack('<HHH', take(data, 46, 6))
    require(stride == 40 and 0 < count < 512 and strings_index < count, 'Invalid ELF section table')
    rows = []
    for index in range(count):
        raw = take(data, offset + index * stride, 40)
        row = dict(zip(('name_offset', 'type', 'flags', 'addr', 'offset', 'size',
                        'link', 'info', 'align', 'entsize'), struct.unpack('<10I', raw)))
        row.update(index=index, raw_hex=raw.hex())
        if row['type'] != 8:  # SHT_NOBITS has no file payload.
            take(data, row['offset'], row['size'])
        rows.append(row)
    strings = rows[strings_index]
    names = take(data, strings['offset'], strings['size'])
    for row in rows:
        row['name'] = cstring(names, row['name_offset'])
    return rows


def symbols(data, section_rows):
    result = []
    for table in section_rows:
        if table['type'] != 2:
            continue
        require(table['entsize'] == 16 and table['size'] % 16 == 0
                and table['size'] // 16 < 200000 and table['link'] < len(section_rows), 'Invalid symbol table')
        strings = section_rows[table['link']]
        names = take(data, strings['offset'], strings['size'])
        for index in range(table['size'] // 16):
            raw = take(data, table['offset'] + index * 16, 16)
            name, value, size, info, other, section = struct.unpack('<IIIBBH', raw)
            result.append({'name': cstring(names, name), 'value': value, 'size': size,
                           'info': info, 'other': other, 'section': section, 'raw_hex': raw.hex()})
    return result


def memory(data, segments, address, count, *, required=True):
    uint(address)
    uint(count)
    require(address + count <= 0x100000000, 'Memory address overflow')
    found = [take(data, p['offset'] + address - p['vaddr'], count) for p in segments
             if p['type'] == 1 and p['vaddr'] <= address
             and count <= p['filesz'] - (address - p['vaddr'])]
    if not found:
        require(not required, 'Missing captured/file-backed memory')
        return None
    require(all(row == found[0] for row in found), 'Overlapping memory extents disagree')
    return found[0]


def prel31(word):
    word &= 0x7fffffff
    return word - 0x80000000 if word & 0x40000000 else word


def direct_bl_target(address, data):
    high, low = struct.unpack('<HH', data)
    require(high & 0xf800 == 0xf000 and low & 0xd000 == 0xd000, 'Expected Thumb BL instruction')
    sign = (high >> 10) & 1
    i1 = 1 ^ ((low >> 13) & 1) ^ sign
    i2 = 1 ^ ((low >> 11) & 1) ^ sign
    immediate = ((sign << 24) | (i1 << 23) | (i2 << 22)
                 | ((high & 0x3ff) << 12) | ((low & 0x7ff) << 1))
    if sign:
        immediate -= 1 << 25
    return address + 4 + immediate


def unwind_stream(word, extab=None):
    """Return EHABI opcodes, retaining CANTUNWIND as a distinct result."""
    if word == 1:
        require(extab is None, 'CANTUNWIND cannot have extab data')
        return None
    if word & 0x80000000:
        require(extab is None and word >> 24 == 0x80, 'Unsupported inline EHABI personality')
        return word.to_bytes(4, 'big')[1:]
    require(extab is not None, 'Missing extab data')
    first = u32(extab, 0)
    if first & 0x80000000:
        personality = (first >> 24) & 0x7f
        require(personality in (0, 1, 2), 'Unsupported compact EHABI personality')
        if personality == 0:
            return first.to_bytes(4, 'big')[1:]
        count = (first >> 16) & 0xff
        result = first.to_bytes(4, 'big')[2:]
        offset = 4
    else:
        first = u32(extab, 4)
        count = first >> 24
        result = first.to_bytes(4, 'big')[1:]
        offset = 8
    require(count <= 16, 'Oversized EHABI opcode stream')
    for index in range(count):
        result += u32(extab, offset + index * 4).to_bytes(4, 'big')
    return result


def unwind_frame(opcodes):
    """Decode the actual retained subset, failing closed on other operations.

    This derives SP movement and the LR slot independently of the diagnosis's
    prologue arithmetic. It is not an exception dispatcher or generic unwinder.
    """
    if opcodes is None:
        return None
    offset, lr_offset, index = 0, None, 0
    while index < len(opcodes):
        op = opcodes[index]
        index += 1
        if op <= 0x3f:
            offset += 4 * (op + 1)
        elif 0xa0 <= op <= 0xaf:
            registers = (op & 7) + 1
            if op & 8:
                require(lr_offset is None, 'Multiple EHABI LR pops')
                lr_offset = offset + 4 * registers
                registers += 1
            offset += 4 * registers
        elif op == 0xc9:
            require(index < len(opcodes) and opcodes[index] in (0x80, 0x81), 'Unsupported EHABI VFP pop')
            offset += 8 * ((opcodes[index] & 15) + 1)
            index += 1
        elif op == 0xb0:
            break
        else:
            raise AudioEvidenceError('Unsupported retained EHABI opcode: ' + hex(op))
    require(offset <= 16384, 'Oversized EHABI frame')
    return {'frame_bytes': offset, 'lr_sp_offset': lr_offset}


def core_snapshot(compressed):
    require(isinstance(compressed, bytes) and 0 < len(compressed) <= 64 * 1024 * 1024, 'Invalid compressed core')
    try:
        with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:
            data = stream.read(MAX_CORE + 1)
    except (OSError, EOFError) as error:
        raise AudioEvidenceError('Invalid compressed core stream') from error
    require(len(data) <= MAX_CORE, 'Expanded core exceeds retention bound')
    segments = program_headers(data)
    notes = {}
    for segment in segments:
        if segment['type'] != 4:
            continue
        offset, end = segment['offset'], segment['offset'] + segment['filesz']
        while offset < end:
            name_size, size, kind = struct.unpack('<III', take(data, offset, 12))
            offset += 12
            require(0 < name_size < 4096 and size <= 16 * 1024 * 1024, 'Invalid core note extent')
            name = cstring(take(data, offset, name_size), 0)
            offset = (offset + name_size + 3) & ~3
            require(offset + size <= end and name not in notes, 'Duplicate or truncated core note')
            notes[name] = take(data, offset, size)
            offset = (offset + size + 3) & ~3
        require(offset == end, 'Unaligned core note extent')
    require(all(name in notes for name in ('MODULE_INFO', 'THREAD_INFO', 'THREAD_REG_INFO')), 'Missing core notes')
    modules, block, offset = [], notes['MODULE_INFO'], 8
    count = u32(block, 4)
    require(count < 128, 'Too many core modules')
    for _ in range(count):
        n = u32(block, offset + 0x4c)
        require(0 < n <= 8, 'Invalid core module segment count')
        take(block, offset, 0x60 + 20 * n)
        module = {'name': cstring(take(block, offset + 0x24, 32), 0), 'segments': []}
        for index in range(n):
            base = offset + 0x50 + 20 * index
            module['segments'].append({'index': index, 'flags': u32(block, base + 4) & 7,
                                       'vaddr': u32(block, base + 8), 'size': u32(block, base + 12)})
        modules.append(module)
        offset += 0x60 + 20 * n
    require(offset <= len(block) and not any(block[offset:]), 'Trailing core module data')
    threads = {}
    for name, minimum in (('THREAD_INFO', 0xa0), ('THREAD_REG_INFO', 0x48)):
        block, offset = notes[name], 8
        count = u32(block, 4)
        require(count <= 256, 'Too many core threads')
        seen = set()
        for _ in range(count):
            size = u32(block, offset)
            require(minimum <= size <= 4096, 'Invalid core thread record size')
            row = take(block, offset, size)
            uid = u32(row, 4)
            require(uid not in seen, 'Duplicate core thread record')
            seen.add(uid)
            if name == 'THREAD_INFO':
                threads[uid] = {'uid': uid, 'stop_reason': u32(row, 0x74),
                                'name': cstring(take(row, 8, 32), 0),
                                'stack_base': u32(row, 0x38), 'stack_size': u32(row, 0x3c)}
            else:
                require(uid in threads, 'Unmatched thread registers')
                threads[uid]['registers'] = list(struct.unpack('<16I', take(row, 8, 64)))
            offset += size
        require(offset == len(block), 'Trailing core thread data')
    return data, segments, modules, threads


def validate_extract(extract, compressed):
    """Recheck a capsule without needing its former full ELF or SELF."""
    require(isinstance(extract, dict) and extract.get('profile') == PROFILE, 'Unsupported audio extract profile')
    require(sha(compressed) == extract['core']['sha256'], 'Audio core identity mismatch')
    header = raw_hex(extract['elf_header_hex'], 52, size=52)
    elf_header(header)
    ph = extract['program_headers']
    require(isinstance(ph, list) and 0 < len(ph) < 512
            and len(ph) == struct.unpack('<H', header[44:46])[0], 'Retained program header count mismatch')
    for row in ph:
        raw = raw_hex(row['raw_hex'], 32, size=32)
        for key, value in zip(('type', 'offset', 'vaddr', 'paddr', 'filesz', 'memsz', 'flags', 'align'), struct.unpack('<8I', raw)):
            require(uint(row[key]) == value, 'Retained program header mismatch')
        require(row['offset'] + row['filesz'] <= extract['elf']['size']
                and row['vaddr'] + row['memsz'] <= 0x100000000, 'Retained program extent overflow')
    load = [row for row in ph if row['type'] == 1]
    functions = extract['functions']
    require(isinstance(functions, list) and len(functions) == 15, 'Audio capsule requires 15 complete functions')
    by_start, labels, ranges, total = {}, set(), [], 0
    for row in functions:
        start, size = uint(row['start']), uint(row['size'], MAX_FUNCTION)
        require(size > 0 and start % 2 == 0 and size % 2 == 0 and start + size <= 0x100000000, 'Invalid function extent')
        label = row['label']
        require(isinstance(label, str) and 0 < len(label) <= 128 and label not in labels, 'Duplicate or invalid function label')
        sym = row['symbol']
        raw_sym = raw_hex(sym['raw_hex'], 16, size=16)
        _, value, length, info, other, section = struct.unpack('<IIIBBH', raw_sym)
        require((value & ~1) == start and value & 1 and length == size and info & 15 == 2
                and all(sym[k] == v for k, v in (('value', value), ('size', length), ('info', info), ('other', other), ('section', section))),
                'Function is not its complete original Thumb STT_FUNC extent')
        code = raw_hex(row['bytes_hex'], MAX_FUNCTION, size=size, digest=row['sha256'])
        matches = [p for p in load if p['flags'] & 1 and p['vaddr'] <= start and start + size <= p['vaddr'] + p['filesz']
                   and row['file_offset'] == p['offset'] + start - p['vaddr']]
        require(matches, 'Function outside executable file-backed segment')
        require(start not in by_start, 'Duplicate function address')
        by_start[start] = (row, code)
        labels.add(label)
        ranges.append((start, start + size))
        total += size
    require(total <= MAX_CODE and total == extract['code_bytes'], 'Invalid total retained code size')
    ranges.sort()
    require(all(a[1] <= b[0] for a, b in zip(ranges, ranges[1:])), 'Overlapping retained functions')
    support = extract['supporting_symbols']
    require(isinstance(support, list) and len(support) == 3, 'Missing supporting audio symbols')
    for symbol in support:
        raw = raw_hex(symbol['raw_hex'], 16, size=16)
        _, value, length, info, other, section = struct.unpack('<IIIBBH', raw)
        require(all(symbol[k] == v for k, v in (('value', value), ('size', length), ('info', info), ('other', other), ('section', section))),
                'Supporting symbol metadata mismatch')
    fragments = extract['data_fragments']
    require(isinstance(fragments, list) and len(fragments) == 1, 'Missing retained manager vtable')
    fragment = fragments[0]
    address, size = uint(fragment['address']), uint(fragment['size'], 4096)
    raw_hex(fragment['bytes_hex'], 4096, size=size, digest=fragment['sha256'])
    require(size == 40 and any(row['value'] == address and row['size'] == size and row['info'] & 15 == 1 for row in support)
            and any(row['vaddr'] <= address and address + size <= row['vaddr'] + row['filesz'] for row in load),
            'Retained vtable is not its original bounded object')

    def code_at(address, count):
        found = [code[address - start:address - start + count] for start, (row, code) in by_start.items()
                 if start <= address and address + count <= start + row['size']]
        require(len(found) == 1, 'Instruction outside retained function bounds')
        return found[0]

    unwind, records = {}, extract['ehabi']
    require(isinstance(records, list) and len(records) == len(functions), 'Incomplete EHABI coverage')
    exidx, extab = extract['ehabi_sections']['exidx'], extract['ehabi_sections']['extab']
    for row in records:
        start, address = uint(row['function_start']), uint(row['entry_address'])
        require(start in by_start and start not in unwind and exidx['addr'] <= address
                and address + 16 <= exidx['addr'] + exidx['size'], 'Invalid EHABI coverage entry')
        raw = raw_hex(row['entry_hex'], 8, size=8)
        first, word = struct.unpack('<II', raw)
        require(address + prel31(first) == start, 'EHABI start does not match function')
        following = raw_hex(row['next_entry_hex'], 8, size=8)
        end = address + 8 + prel31(u32(following, 0))
        require(row['covered_until'] == end and start + by_start[start][0]['size'] <= end, 'EHABI does not cover complete function')
        data = None
        if word != 1 and not word & 0x80000000:
            saved = row['extab']
            ea, limit = uint(saved['address']), uint(saved['limit'])
            require(ea == address + 4 + prel31(word) and extab['addr'] <= ea < limit <= extab['addr'] + extab['size'], 'Invalid extab extent')
            ref_addr = uint(saved['next_reference_address'])
            ref = raw_hex(saved['next_reference_hex'], 8, size=8)
            ref_word = u32(ref, 4)
            require(exidx['addr'] <= ref_addr and ref_addr + 8 <= exidx['addr'] + exidx['size']
                    and ref_word != 1 and not ref_word & 0x80000000
                    and ref_addr + 4 + prel31(ref_word) == limit, 'Extab boundary is not the retained next reference')
            data = raw_hex(saved['bytes_hex'], 4096, size=limit - ea, digest=saved['sha256'])
        else:
            require(row.get('extab') is None, 'Unexpected extab evidence')
        stream = unwind_stream(word, data)
        decoded = unwind_frame(stream)
        require(row['opcodes_hex'] == (stream.hex() if stream is not None else None)
                and row['decoded'] == decoded, 'EHABI decoded evidence mismatch')
        unwind[start] = decoded

    data, core_ph, modules, threads = core_snapshot(compressed)
    app = [row for row in modules if row['name'] == 'strikers']
    require(len(app) == 1 and len(app[0]['segments']) == len(load), 'Core/ELF module segment mismatch')
    relocations = []
    for runtime, elf in zip(app[0]['segments'], load):
        require(runtime['flags'] == elf['flags'], 'Core/ELF segment flags mismatch')
        relocations.append({'runtime_start': runtime['vaddr'], 'elf_start': elf['vaddr'],
                            'runtime_size': runtime['size'], 'elf_filesz': elf['filesz'],
                            'elf_memsz': elf['memsz'], 'delta': runtime['vaddr'] - elf['vaddr']})
    require(extract['relocations'] == relocations, 'Retained relocation differs from raw core')

    def to_elf(address):
        value = address & ~1
        found = [row['elf_start'] + value - row['runtime_start'] for row in relocations
                 if row['runtime_start'] <= value < row['runtime_start'] + row['elf_memsz']]
        require(len(found) == 1, 'Runtime address outside matched ELF segments')
        return found[0]

    proof = extract['proof']
    thread = threads.get(uint(proof['thread_uid']))
    require(thread is not None and thread['stop_reason'] == proof['stop_reason'] == 0x00030004, 'Fault thread/stop reason mismatch')
    regs = thread.get('registers')
    require(regs is not None and regs == proof['registers'], 'Registers differ from raw core')
    pc = to_elf(regs[15])
    require(pc == proof['pc_elf'] and regs[5] == 0, 'Expected NULL this-pointer at retained PC')
    require(code_at(pc, 2) == b'\x6c\x6e' and code_at(proof['this_move_elf'], 2) == b'\x05\x46', 'Fault/provenance instruction mismatch')
    chain = proof['chain']
    require(isinstance(chain, list) and len(chain) == 7 and chain[0]['sp'] == regs[13], 'Invalid reviewed chain length or initial SP')
    first = by_start.get(chain[0]['function_start'])
    require(first is not None and first[0]['start'] <= pc < first[0]['start'] + first[0]['size'], 'Fault PC is outside the first reviewed frame')
    direct, expected_sp = 0, regs[13]
    for frame in chain:
        start, sp = uint(frame['function_start']), uint(frame['sp'])
        decoded = unwind.get(start)
        require(decoded is not None and decoded['lr_sp_offset'] is not None, 'Reviewed frame lacks usable EHABI')
        require(sp == expected_sp and frame['frame_bytes'] == decoded['frame_bytes'], 'Frame SP differs from EHABI')
        slot = sp + decoded['lr_sp_offset']
        require(slot == frame['saved_lr_slot'], 'Saved LR slot differs from EHABI')
        lr = u32(memory(data, core_ph, slot, 4), 0)
        require(lr == frame['saved_lr_runtime'] and lr & 1, 'Saved LR differs from raw core')
        call, count = uint(frame['callsite_elf']), uint(frame['call_size'], 4)
        require(count in (2, 4) and to_elf(lr) == call + count, 'Callsite/return alignment mismatch')
        insn = code_at(call, count)
        require(insn.hex() == frame['call_bytes_hex'], 'Callsite instruction evidence mismatch')
        if count == 4:
            require(direct_bl_target(call, insn) == start, 'BL does not target the reviewed callee')
            direct += 1
        else:
            require(struct.unpack('<H', insn)[0] & 0xff87 == 0x4780, 'Expected indirect Thumb BLX')
        expected_sp += decoded['frame_bytes']
    require(direct == 5, 'Unexpected direct/indirect reviewed call count')
    require(u32(memory(data, core_ph, proof['null_argument_slot'], 4), 0) == 0, 'Captured caller argument is not NULL')
    manager = proof['manager']
    candidate = uint(manager['address'])
    require(u32(memory(data, core_ph, manager['caller_slot'], 4), 0) == candidate, 'Manager candidate slot changed')
    raw = memory(data, core_ph, candidate, 0x7c)
    require(u32(raw, 0) == manager['vptr_runtime'] and u32(raw, 0x74) == 0
            and u32(raw, 0x78) == 3 and u32(raw, 0x70) == manager['table_pointer'], 'Captured candidate manager differs')
    require(to_elf(manager['vptr_runtime']) == manager['vtable_elf'] + 8, 'Candidate vtable relocation mismatch')
    require(manager['vtable_elf'] == fragment['address'], 'Candidate vtable is not retained')
    require(memory(data, core_ph, manager['global_pointer_runtime'], 4, required=False) is None
            and manager['global_pointer_captured'] is False, 'Manager-global observation limit changed')
    windows = extract['core_windows']
    require(isinstance(windows, list) and 1 <= len(windows) <= 16, 'Invalid retained core window count')
    for window in windows:
        raw = raw_hex(window['bytes_hex'], 8192, size=window['size'], digest=window['sha256'])
        require(memory(data, core_ph, uint(window['address']), len(raw)) == raw, 'Retained memory window differs from core')
    return {'functions': len(functions), 'code_bytes': total, 'verified_frames': len(chain),
            'direct_calls': direct, 'indirect_calls': len(chain) - direct,
            'core_windows': len(windows), 'expanded_core_bytes': len(data),
            'hardware_status': 'unresolved'}
