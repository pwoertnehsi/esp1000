# Vendored TinyUSB subset

Upstream: https://github.com/hathach/tinyusb (MIT; see LICENSE).

Copied from the existing local `psp_cam_impersonator` project's
`espressif__esp_tinyusb` 3.0.1 component, on 2026-11-15. Its embedded Git pointer
is unavailable, so an exact upstream commit has not been established. Source
files are unmodified. `SHA256SUMS` records the imported subset.

Only common, OS abstraction, host core and Synopsys DWC2 sources are retained.
`config/` and `CMakeLists.txt` are this project's integration. No device stack,
camera impersonation, registry fetch, or ESP-IDF enumeration patch is used.
