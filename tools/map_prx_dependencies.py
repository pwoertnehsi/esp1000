#!/usr/bin/env python3
"""Resolve function imports by exact library+NID in a PRX inventory.

Missing providers may be ordinary firmware modules outside the input set.
Resolution does not prove a prototype, calling convention or runtime linkage.
"""
import argparse
import json
from pathlib import Path


def resolve(modules):
    providers = {}
    for module in modules:
        for table in module['exports']:
            if table['library'] == 'syslib':
                continue
            for entry in table['entries']:
                if entry['kind'] == 'function':
                    key = table['library'], entry['nid']
                    providers.setdefault(key, []).append({
                        'file': module['file'], 'module': module['module'],
                        'address': entry['address'], 'sha256': module['sha256']})
    imports = []
    for module in modules:
        for table in module['imports']:
            for entry in table['entries']:
                imports.append({'consumer': module['file'], 'library': table['library'],
                                'nid': entry['nid'], 'stub': entry['address'],
                                'name': entry.get('sdk_name'),
                                'providers': providers.get((table['library'], entry['nid']), [])})
    return imports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inventory', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    records = resolve(json.loads(args.inventory.read_text()))
    args.output.write_text(json.dumps(records, indent=2) + '\n')
    print(f'{len(records)} function imports; '
          f'{sum(bool(r["providers"]) for r in records)} resolve within supplied modules')


if __name__ == '__main__':
    main()
