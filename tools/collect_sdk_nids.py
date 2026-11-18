#!/usr/bin/env python3
"""Annotate PRX NIDs from installed PSPSDK stubs (requires pyelftools).

Names describe SDK stubs, not recovered private prototypes. Preserve multiple
names for alias NIDs. No firmware files or SDK archives are modified.
"""
import argparse
from io import BytesIO
import json
from pathlib import Path
import struct
import subprocess
from elftools.elf.elffile import ELFFile


def collect(archives):
    names = {}
    for archive in archives:
        members = subprocess.check_output(["psp-ar", "t", str(archive)], text=True).splitlines()
        for member in members:
            data = subprocess.check_output(["psp-ar", "p", str(archive), member])
            elf = ELFFile(BytesIO(data))
            nids = elf.get_section_by_name(".rodata.sceNid")
            symbols = elf.get_section_by_name(".symtab")
            if nids is None or symbols is None or nids.data_size != 4:
                continue
            functions = [s.name for s in symbols.iter_symbols()
                         if s['st_info']['type'] == 'STT_FUNC'
                         and s['st_info']['bind'] == 'STB_GLOBAL'
                         and s['st_shndx'] != 'SHN_UNDEF']
            if len(functions) != 1:
                continue
            nid = f"{struct.unpack('<I', nids.data())[0]:08X}"
            names.setdefault(nid, set()).add(functions[0])
    return {nid: " / ".join(sorted(values)) for nid, values in sorted(names.items())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = collect(args.archives)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"Collected {len(result)} SDK NID annotations")


if __name__ == "__main__":
    main()
