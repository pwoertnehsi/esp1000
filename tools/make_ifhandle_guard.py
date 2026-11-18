#!/usr/bin/env python3
"""Generate non-relocated 6.61 ABI anchors; never patches the input PRX."""
from pathlib import Path
import argparse
from inspect_prx import Prx

# Stack/field accesses and constants recovered from actual call sites. No LUI
# addresses or J/JAL targets: those are changed by the PSP relocator.
ANCHORS = [0x968, 0x974, 0x984, 0x9e8, 0x9f0, 0xa00, 0xa34, 0xa58,
           0xa78, 0xa84, 0x1060, 0x1198, 0x11a4, 0x11a8,
           0x11d0, 0x11d4, 0x11d8, 0x11dc, 0x2ac, 0x2b0, 0x2b8,
           0xa94, 0xaa4, 0xaa8, 0xab0,
           0x19bc, 0x19fc, 0x1a08, 0x1a10, 0x1b50, 0x1b88,
           0x3b4, 0x14f0, 0x1500]
NIDS = {'create': 0x16042084, 'attach': 0xAE81C0CB,
        'detach': 0x54D1AEA1, 'destroy': 0xC9344A59, 'ioctl': 0x0296C7D6, 'complete': 0xF94BAF52,
        'dequeue': 0xE2F4F1C9, 'enqueue': 0xC28F6FF2,
        'mget': 0x59F0D619, 'mfreem': 0xF56FAC82}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    image = Prx(args.input, {})
    report = image.report()
    if report['module'] != 'sceNet_Service' or image.segments[0]['filesz'] != 0x3cf0:
        raise ValueError('Unexpected ifhandle image')
    exports = {e['nid']: int(e['address'], 16) for t in report['exports']
               if t['library'] == 'sceNetIfhandle_driver' for e in t['entries']}
    lines = ['/* Generated from SHA-256 ' + report['sha256'] + '.',
             ' * These are ABI anchors, not a complete runtime cryptographic hash. */',
             '#ifndef PSP_IFHANDLE_661_GUARD_H', '#define PSP_IFHANDLE_661_GUARD_H',
             'static const struct { unsigned offset, expected; } ifhandle_anchors[] = {']
    for address in ANCHORS:
        lines.append(f'    {{0x{address:04x}u, 0x{image.word(address):08x}u}},')
    lines += ['};']
    for name, nid in NIDS.items():
        lines += [f'#define IF_{name.upper()}_NID 0x{nid:08x}u',
                  f'#define IF_{name.upper()}_OFFSET 0x{exports[f"0x{nid:08X}"]:04x}u']
    lines += ['#endif', '']
    args.output.write_text('\n'.join(lines))


if __name__ == '__main__':
    main()
