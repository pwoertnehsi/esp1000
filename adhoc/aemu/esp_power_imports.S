    .set noreorder
#include "pspimport.s"
    IMPORT_START "esp1000_power",0x40010000
    IMPORT_FUNC "esp1000_power",0x1,espAdhocPowerAcquire
    IMPORT_FUNC "esp1000_power",0x2,espAdhocPowerRelease
