# Vendored TinyUSB subset

Upstream: https://github.com/hathach/tinyusb (MIT; see LICENSE).

Copied from the existing local `psp_cam_impersonator` project's
`espressif__esp_tinyusb` 3.0.1 component, on 2026-11-15. Its embedded Git pointer
is unavailable, so an exact upstream commit has not been established. `SHA256SUMS` records the original imported subset.

Local modification (2026-11-18): `src/host/usbh.c` adds an optional weak
`tuh_enum_diagnostic_cb` observer at enumeration begin, delay dispatch, control
completion and finish. It exposes stage/result/count metadata only and changes
no enumeration decisions or delays. Synthetic transfers never expose their
uninitialized `actual_len`. `LOCAL_SHA256SUMS` records modified source hashes.

Only common, OS abstraction, host core and Synopsys DWC2 sources are retained.
`config/` and `CMakeLists.txt` are this project's integration. No device stack,
camera impersonation, registry fetch, or ESP-IDF enumeration patch is used.
