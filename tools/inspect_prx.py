#!/usr/bin/env python3
"""Read sectionless decrypted PSP PRX import/export tables without modification.

Addresses are pre-relocation module-relative virtual addresses, not runtime
patch addresses. NID names are optional annotations, not ABI/prototype proof.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


class Prx:
    def __init__(self, path, names):
        self.path, self.names = Path(path), names
        self.data = self.path.read_bytes()
        if self.data[:7] != b"\x7fELF\x01\x01\x01":
            raise ValueError(f"{path}: requires little-endian ELF32")
        if self.u16(18) != 8:
            raise ValueError("not MIPS")
        self.segments = []
        phoff, size, count = self.u32(28), self.u16(42), self.u16(44)
        for i in range(count):
            fields = struct.unpack_from("<8I", self.data, phoff + i * size)
            if fields[0] == 1:
                self.segments.append(dict(zip(
                    ("type", "offset", "vaddr", "paddr", "filesz", "memsz", "flags", "align"), fields)))
        # PSP PRX uses first PT_LOAD p_paddr (bit31=kernel) as FILE offset
        # of SceModuleInfo; it is not an ordinary loadable physical address.
        self.info = self.segments[0]["paddr"] & 0x7fffffff
        self.read(self.info, 52)

    def read(self, offset, length):
        if offset < 0 or length < 0 or offset + length > len(self.data):
            raise ValueError(f"out-of-bounds read {offset:x}+{length:x}")
        return self.data[offset:offset + length]

    def u16(self, offset):
        return struct.unpack("<H", self.read(offset, 2))[0]

    def u32(self, offset):
        return struct.unpack("<I", self.read(offset, 4))[0]

    def file_offset(self, address, length=1):
        for segment in self.segments:
            displacement = address - segment["vaddr"]
            if 0 <= displacement and displacement + length <= segment["filesz"]:
                return segment["offset"] + displacement
        raise ValueError(f"address 0x{address:x} is not file backed")

    def word(self, address):
        return self.u32(self.file_offset(address, 4))

    def string(self, address):
        if not address:
            return "syslib"
        offset = self.file_offset(address)
        stop = self.data.find(b"\0", offset, offset + 256)
        if stop < 0:
            raise ValueError("unterminated library name")
        return self.data[offset:stop].decode("ascii")

    def entry(self, nid, address, variable=False):
        result = {"nid": f"0x{nid:08X}", "address": f"0x{address:08X}",
                  "kind": "variable" if variable else "function"}
        if f"{nid:08X}" in self.names:
            result["sdk_name"] = self.names[f"{nid:08X}"]
        return result

    def tables(self, start, stop, imports):
        result = []
        while start < stop:
            offset = self.file_offset(start, 16)
            name, version, flags, words, variables, functions, table = struct.unpack(
                "<IHHBBHI", self.read(offset, 16))
            if words < (5 if imports else 4) or start + words * 4 > stop:
                raise ValueError("invalid library table length")
            entries = []
            if imports:
                stub = self.u32(offset + 16)
                for i in range(functions):
                    entries.append(self.entry(self.word(table + 4 * i), stub + 8 * i))
                # Variable relocation stubs differ from function trampolines;
                # report their count without guessing their structure.
            else:
                count = functions + variables
                for i in range(count):
                    entries.append(self.entry(self.word(table + 4 * i),
                                              self.word(table + 4 * (count + i)), i >= functions))
            result.append({"library": self.string(name), "table_address": f"0x{start:08X}",
                           "version": f"0x{version:04X}", "flags": f"0x{flags:04X}",
                           "function_count": functions, "variable_count": variables, "entries": entries})
            start += words * 4
        if start != stop:
            raise ValueError("library table end mismatch")
        return result

    def report(self):
        info = self.info
        exports, export_end, imports, import_end = struct.unpack("<4I", self.read(info + 36, 16))
        return {"file": str(self.path), "sha256": hashlib.sha256(self.data).hexdigest(),
                "file_bytes": len(self.data), "module": self.data[info+4:info+32].split(b"\0")[0].decode(),
                "module_attributes": f"0x{self.u16(info):04X}",
                "module_version_bytes": list(self.read(info + 2, 2)),
                "elf_entry": f"0x{self.u32(24):08X}", "segments": self.segments,
                "exports": self.tables(exports, export_end, False),
                "imports": self.tables(imports, import_end, True)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+")
    parser.add_argument("--nid-map", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    names = json.loads(args.nid_map.read_text()) if args.nid_map else {}
    reports = [Prx(path, names).report() for path in args.files]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(reports, indent=2) + "\n")
    for report in reports:
        print(f'{report["file"]}: {report["module"]}; '
              f'{sum(x["function_count"] for x in report["exports"])} exported functions; '
              f'{sum(x["function_count"] for x in report["imports"])} imported functions')


if __name__ == "__main__":
    main()
