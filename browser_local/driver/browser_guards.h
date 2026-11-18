/* Generated from local 6.61 PRXs; selected guards, not a full runtime hash. */
#ifndef PSP_BROWSER_GUARDS_H
#define PSP_BROWSER_GUARDS_H
static const browser_path_t browser_paths_0[] = {
    {0x9d64u, "flash0:/vsh/module/htmlviewer_ui.prx", "brows0:/vsh/module/htmlviewer_ui.prx"},
    {0x9df0u, "flash0:/vsh/module/netfront.prx", "brows0:/vsh/module/netfront.prx"},
    {0x9e64u, "flash0:/vsh/module/libslim.prx", "brows0:/vsh/module/libslim.prx"},
    {0x9e8cu, "flash0:/vsh/module/libwww.prx", "brows0:/vsh/module/libwww.prx"},
    {0x9e3cu, "flash0:/vsh/module/mm_flash.prx", "brows0:/vsh/module/mm_flash.prx"},
};
static const browser_anchor_t browser_anchors_0[] = {
    {0x2370u,0x27bdffe0u},
    {0x2378u,0xafb3000cu},
    {0x2380u,0xafb20008u},
    {0x238cu,0xafb10004u},
};
static const browser_path_t browser_paths_1[] = {
    {0xd040u, "flash0:/vsh/resource/", "ms0:/b0/vsh/resource/"},
};
static const browser_anchor_t browser_anchors_1[] = {
    {0x23e0u,0x27bdfff0u},
    {0x23e4u,0xafb10004u},
    {0x23ecu,0xafb00000u},
    {0x23f0u,0xafbf0008u},
};
static const browser_guard_t browser_guards[] = {
    {"sceUtility_Driver",2u,1u,0xb038u,browser_paths_0,5u,browser_anchors_0,4u},
    {"sceVshHVUtility_Module",1u,1u,0xe808u,browser_paths_1,1u,browser_anchors_1,4u},
};
#endif
