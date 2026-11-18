#include <pspkernel.h>
#include <psploadcore.h>
#include <kubridge.h>
#include <systemctrl.h>
#include "native_config.h"

vnet_ioctl_fn vnet_resolve_ioctl(void)
{
    SceModule mod;
    if (kuKernelFindModuleByName("sceNetInet_Library", &mod) < 0
        || mod.text_size != 0x31d70 || mod.version[0] != 3 || mod.version[1] != 1)
        return 0;
    unsigned address = sctrlHENFindFunction("sceNetInet_Library", "sceNetInet_lib", 0xaee60f84);
    if (address != mod.text_addr + 0x8848) return 0;
    /* The same 3-argument ioctl entry used by Sony's address wrapper.
     * No dependency on its preliminary delete-address operation. */
    static const struct { unsigned offset, word; } anchors[] = {
        {0, 0x27bdff50}, {8, 0x00808021}, {0x14, 0x00c0b021}, {0x1c, 0x00a09821}
    };
    for (unsigned i = 0; i < sizeof(anchors) / sizeof(anchors[0]); ++i)
        if (*(volatile unsigned *)(address + anchors[i].offset) != anchors[i].word) return 0;
    return (vnet_ioctl_fn)address;
}
