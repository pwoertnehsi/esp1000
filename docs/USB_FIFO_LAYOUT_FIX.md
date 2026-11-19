# ESP22 USB host FIFO layout correction

The externally powered Gran Turismo capture recorded eight five-second bulk-OUT
stalls, continuing bulk-IN traffic, successful Wi-Fi association and recurring
invalid incoming records. Inspecting the vendored DWC2 driver found a concrete
FIFO RAM allocation defect, independent of power and port-map recovery.

The ESP32-S3 has 256 FIFO RAM words. RX always starts at word zero. The imported
allocator subtracted RX size from its top-down allocation cursor before placing
the TX regions, causing periodic TX to start at zero and non-periodic TX to
sit inside RX storage. In the previous eight-channel slave configuration, RX
occupied [0,139), periodic TX [0,85), and bulk/control TX [85,117). These regions
must not overlap. Concurrent receive/transmit storage can therefore collide;
the capture alone does not establish that this explains every observed stall.

ESP22 removes the extra RX subtraction and uses the actual channel count
instead of the hardware's encoded count-minus-one when sizing RX/DMA storage.
The resulting slave-mode layout is RX [0,140), periodic TX [140,224), and
bulk/control TX [224,256), measured in 32-bit words. This matches the allocation
ordering in [upstream TinyUSB](https://github.com/hathach/tinyusb/blob/master/src/portable/synopsys/dwc2/hcd_dwc2.c).
Only the allocator correction and read-only layout diagnostics change in the
USB controller code; endpoint and retry behavior, watchdog timeout, Ad Hoc
mapping recovery, PSP modules and accessory-power behavior are retained.

`tests/test_dwc2_fifo_layout.py` compiles the actual allocator with mocked
registers under AddressSanitizer and UndefinedBehaviorSanitizer. The old code
fails the RX/TX non-overlap assertion; the corrected code passes 65 full/high-
speed and slave/DMA configurations, including the exact ESP32-S3 layout.
It checks boundaries, region separation, channel reservation and simultaneous
buffer ownership. `make test` passes eight standalone C sanitizer programs and
55 Python tests. The ESP-IDF build succeeds without new warnings.

The prior ESP21 images and new ESP22 images are preserved in the ignored local
`build/esp22-fifo-layout` directory, together with failing/passing test and
build/flash logs. Bootloader and partition table are unchanged. ESP22 app SHA256:
`5bca49d4070687e9d454c5eb3d8e26f0bdcccce201b37824beb46fa1c6d70662`.
The `FIFO_LAYOUT` status record reports actual programmed registers so the
hardware layout can be checked after flashing. A successful race, sustained
traffic without watchdog resets and clean exit still require hardware testing.

Post-flash UART status confirms actual RX size 140, periodic TX start 140 /
size 84, and non-periodic TX start 224 / size 32. Flash readback hashes were
verified by esptool; the idle post-flash watchdog counter is zero. This verifies
installation and layout, not race stability.
