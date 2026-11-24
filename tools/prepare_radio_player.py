#!/usr/bin/env python3
"""Make a private local-radio launcher from an owner's existing Sony PRS.

Preserves its icon and container layout. No downloaded or copied Sony assets
are included in this repository. Existing installed launchers are never edited.
"""
import argparse
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
LOCAL_URL = 'file:/PSP/RADIOPLAYER/esp1000-radio/index.html'


def rewrite(template, replacements):
    if len(template) < 64 or template[:8] != b'PRSF\x00\x01\x00\x00':
        raise ValueError('Expected a version-1 PRSF launcher')
    nodes, nodes_size = struct.unpack_from('<II', template, 8)
    strings, strings_size = struct.unpack_from('<II', template, 24)
    if nodes < 64 or nodes % 4 or nodes_size % 4 or nodes + nodes_size > strings or strings + strings_size > len(template):
        raise ValueError('Invalid PRSF table bounds')
    pool = template[strings:strings + strings_size]
    edits = []
    for key, value in replacements.items():
        encoded = value.encode('utf-8')
        if b'\0' in encoded:
            raise ValueError('Embedded NUL in replacement')
        matches = []
        for at in range(nodes, nodes + nodes_size - 15, 4):
            name, kind, offset, length = struct.unpack_from('<4I', template, at)
            if kind != 3 or name >= strings_size or pool.find(b'\0', name) < 0:
                continue
            if pool[name:pool.index(b'\0', name)] != key.encode('ascii'):
                continue
            if offset + length >= strings_size or pool[offset + length] != 0:
                raise ValueError(f'Invalid string bounds for {key}')
            matches.append((at, offset, length))
        if len(matches) != 1:
            raise ValueError(f'Expected one string property: {key}')
        at, offset, length = matches[0]
        if len(encoded) > length:
            raise ValueError(f'{key} does not fit the template slot ({length} bytes)')
        edits.append((at, offset, length, encoded))
    # Reject ambiguous/overlapping properties before making any changes.
    spans = sorted((offset, offset + length + 1) for _, offset, length, _ in edits)
    if any(left[1] > right[0] for left, right in zip(spans, spans[1:])):
        raise ValueError('Overlapping PRSF string properties')
    data = bytearray(template)
    for at, offset, length, encoded in edits:
        data[strings + offset:strings + offset + length + 1] = encoded + bytes(length + 1 - len(encoded))
        struct.pack_into('<I', data, at + 12, len(encoded))
    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--template', type=Path, required=True, help='An existing owner-supplied Sony .prs file')
    parser.add_argument('--output', type=Path, default=ROOT/'build/direct-radio')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Use a fresh output directory')
    data = rewrite(args.template.read_bytes(), {
        'title': 'ESP1000 Direct Radio', 'radioplayer_url': LOCAL_URL,
        'comment': 'Direct-stream test player. No station directory.',
        'author': 'ESP', 'copyright': 'ESP1000 test; original icon: Sony',
    })
    player = args.output/'PSP/RADIOPLAYER'
    (player/'esp1000-radio').mkdir(parents=True)
    (player/'ESP1000_Direct.prs').write_bytes(data)
    (player/'esp1000-radio/index.html').write_bytes((ROOT/'radio_player/index.html').read_bytes())
    print(f'Prepared {player}; copy its contents to PSP/RADIOPLAYER/')
    print('Launch ESP1000 Direct Radio from XMB Internet Radio, not as an ordinary browser page.')


if __name__ == '__main__':
    main()
