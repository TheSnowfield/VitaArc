"""Read versionName from an APK's binary AndroidManifest.xml (no Android SDK)."""
import re
import struct


def version_name(data):
    if len(data) > 1024 * 1024 or len(data) < 8:
        raise ValueError('Invalid manifest size')
    kind, header, total = struct.unpack_from('<HHI', data)
    if kind != 3 or header != 8 or total != len(data):
        raise ValueError('Expected binary Android XML')
    strings = []
    cursor = header
    while cursor < total:
        if cursor + 8 > total:
            raise ValueError('Truncated manifest chunk')
        kind, header, size = struct.unpack_from('<HHI', data, cursor)
        end = cursor + size
        if header < 8 or size < header or end > total:
            raise ValueError('Invalid manifest chunk')
        if kind == 1:
            if header < 28:
                raise ValueError('Invalid XML string pool')
            count, _, flags, start, _ = struct.unpack_from('<IIIII', data, cursor + 8)
            if count > (size - header) // 4 or start < header + count * 4 or start > size:
                raise ValueError('Invalid XML string offsets')
            utf8 = bool(flags & 256)

            def length_at(pos):
                width = 1 if utf8 else 2
                if pos + width > end:
                    raise ValueError('Truncated XML string length')
                value = int.from_bytes(data[pos:pos+width], 'little')
                pos += width
                high = 1 << (width * 8 - 1)
                if value & high:
                    if pos + width > end:
                        raise ValueError('Truncated long XML string length')
                    value = ((value & (high - 1)) << (width * 8)) | int.from_bytes(data[pos:pos+width], 'little')
                    pos += width
                return value, pos

            for index in range(count):
                offset = struct.unpack_from('<I', data, cursor + header + index*4)[0]
                length, pos = length_at(cursor + start + offset)
                if utf8:
                    length, pos = length_at(pos)
                finish = pos + length * (1 if utf8 else 2)
                terminator = 1 if utf8 else 2
                if finish + terminator > end or any(data[finish:finish+terminator]):
                    raise ValueError('Invalid XML string extent')
                strings.append(data[pos:finish].decode('utf-8' if utf8 else 'utf-16-le'))
        elif kind == 0x102:
            base = cursor + header
            if base + 20 > end:
                raise ValueError('Truncated XML tag')
            name = struct.unpack_from('<I', data, base + 4)[0]
            if name >= len(strings):
                raise ValueError('Invalid XML tag name')
            if strings[name] == 'manifest':
                start, width, count = struct.unpack_from('<HHH', data, base + 8)
                if start < 20 or width < 20 or base + start + width*count > end:
                    raise ValueError('Invalid manifest attributes')
                for index in range(count):
                    _, key, raw, _, _, typed, value = struct.unpack_from('<IIIHBBI', data, base + start + index*width)
                    if key >= len(strings):
                        raise ValueError('Invalid attribute name')
                    if strings[key] == 'versionName':
                        value = raw if raw != 0xffffffff else value if typed == 3 else 0xffffffff
                        if value >= len(strings) or not re.fullmatch(r'[0-9]+(?:\.[0-9]+){1,3}[A-Za-z0-9._-]*', strings[value]) or len(strings[value]) > 63:
                            raise ValueError('Invalid APK versionName')
                        return strings[value]
        cursor = end
    raise ValueError('Manifest versionName is missing')
