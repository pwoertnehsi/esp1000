#!/usr/bin/env python3
"""Copy the built ESP flash bundle with paths preserved for @flash_args."""
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parent.parent
build = root / "esp_link" / "build"
destination = root / "artifacts" / "esp"
config = json.loads((build / "flasher_args.json").read_text())
for name in config["flash_files"].values():
    relative = Path(name)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"Unexpected flash path: {name}")
    output = destination / relative
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / relative, output)
shutil.copy2(build / "flash_args", destination / "flash_args")
shutil.make_archive(str(root / "artifacts" / "esp-link-test"), "zip", destination)
print(f"ESP flash bundle: {destination}")
