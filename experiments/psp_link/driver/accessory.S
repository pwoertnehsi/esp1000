/* PSPSDK on this machine predates the accessory internal stubs.
 * NIDs: pspdev/psp-ghidra-scripts/ppsspp_niddb.xml. */
#include <pspimport.s>
IMPORT_START sceUsbAcc_internal, 0x00010000
IMPORT_FUNC sceUsbAcc_internal, 0x2E251404, sceUsbAccRegisterType
IMPORT_FUNC sceUsbAcc_internal, 0x18B04C82, sceUsbAccUnregisterType
