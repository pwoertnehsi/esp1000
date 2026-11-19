#!/usr/bin/env python3
"""Copy an ESP-IDF flash bundle with paths preserved for @flash_args."""
import argparse
import json
from pathlib import Path
import shutil


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=root/'esp_link/build')
    parser.add_argument('--output', type=Path, default=root/'artifacts/esp')
    args = parser.parse_args()
    build = args.build.resolve()
    destination = args.output.resolve()
    config = json.loads((build/'flasher_args.json').read_text())
    for name in config['flash_files'].values():
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError(f'Unexpected flash path: {name}')
        output = destination/relative
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(build/relative, output)
    shutil.copy2(build/'flash_args', destination/'flash_args')
    shutil.make_archive(str(destination), 'zip', destination)
    print(f'ESP flash bundle: {destination}')


if __name__ == '__main__':
    main()
