    .set noreorder
#include "pspimport.s"
    IMPORT_START "sceSyscon_driver",0x40010000
    IMPORT_FUNC "sceSyscon_driver",0x3987FEA3,sceSysconCtrlUsbPower
