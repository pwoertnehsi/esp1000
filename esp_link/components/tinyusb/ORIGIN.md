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

Local extension (2026-11-18): `hcd_dwc2.c` calls an optional bounded RX observer
before/after FIFO copies and exports a read-only channel snapshot. The project
observer only inspects its own receive buffer, in the ISR without logging or
blocking. `diagnostics.h` is the project's diagnostic interface. Controller
registers, buffers and transfer acceptance/retry decisions are not modified.

Local correction (2026-11-18, ESP22): the imported host FIFO allocator deducted
RX storage from the top-of-RAM cursor before placing TX regions, even though RX
starts at hardware address zero. This placed both TX FIFOs over RX memory.
Remove that deduction and use the actual channel count for RX/DMA reservations,
as in current upstream TinyUSB's `dfifo_host_init`. On ESP32-S3 slave mode with
eight channels and 256 RAM words: RX [0,140), periodic TX [140,224), non-periodic
TX [224,256). A read-only FIFO register snapshot permits runtime verification.
The actual allocator is compiled in a host sanitizer regression test across
full/high-speed and DMA/slave configurations; the prior allocator fails the
non-overlap assertion. No endpoint/PID/retry or timeout policy is changed.
