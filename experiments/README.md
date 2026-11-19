# Standalone experiments

These applications are optional diagnostics, separate from the production XMB
and GAME plugins. Their runtime filenames and memory-stick paths are unchanged.

- psp_link/: original USB demo and file-transfer application/driver.
- net_probe/: inventory and native module/socket probe.

Reusable code is in common/: page_file.c/h implements the staged file writer;
native_check.c/h implements the native UDP/socket smoke test. Production/shared
sources and tests no longer include code from these experiment directories.

Build explicitly with `make psp` or `make probe` from the repository root. Bare
`make`, `make production` and `make debug` build the current plugins instead.
