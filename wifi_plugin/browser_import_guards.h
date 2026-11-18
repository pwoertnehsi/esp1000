/* Exact owned 6.61 browser import metadata; no firmware bytes distributed. */
static const browser_import_library_t browser_libraries_0[]={
    {0x1aa9c,0x1aca8,0x90011,0x70005,0x1afa4,0x1a45c,"sceNetApctl"},
    {0x1aab0,0x1acb8,0x90011,0x20005,0x1afc0,0x1a494,"sceNetApctl_lib2"},
};
static const browser_import_entry_t browser_entries_0[]={
    {0x1a45c,0x1afa4,0x2935c45b,0x95c,WIFI_NATIVE_ENTRY,"sceNetApctl"},
    {0x1a464,0x1afa8,0x2befdf23,0x9e4,WIFI_NATIVE_INFO,"sceNetApctl"},
    {0x1a46c,0x1afac,0x5963991b,0x57c,WIFI_NATIVE_DEL,"sceNetApctl"},
    {0x1a474,0x1afb0,0x5deac81b,0xb7c,WIFI_NATIVE_STATE,"sceNetApctl"},
    {0x1a47c,0x1afb4,0x8abadd51,0x4fc,WIFI_NATIVE_ADD,"sceNetApctl"},
    {0x1a484,0x1afb8,0xa3e77e13,0x76c,WIFI_NATIVE_SCAN,"sceNetApctl"},
    {0x1a48c,0x1afbc,0xf25a5006,0x858,WIFI_NATIVE_LIST,"sceNetApctl"},
    {0x1a494,0x1afc0,0x4c19731f,0xa60,WIFI_NATIVE_LOGSET,"sceNetApctl_lib2"},
    {0x1a49c,0x1afc4,0x69745f0a,0x1040,WIFI_NATIVE_LOGPTR,"sceNetApctl_lib2"},
};
static const browser_import_guard_t browser_guard_0={"htmlviewer_plugin_module",1,1,0x1f700,browser_libraries_0,sizeof(browser_libraries_0)/sizeof(browser_libraries_0[0]),browser_entries_0,sizeof(browser_entries_0)/sizeof(browser_entries_0[0])};
static const browser_import_library_t browser_libraries_1[]={
    {0x5bc48,0x5be14,0x90011,0x50005,0x5c50c,0x5bb2c,"sceNetApctl"},
};
static const browser_import_entry_t browser_entries_1[]={
    {0x5bb2c,0x5c50c,0x2befdf23,0x9e4,WIFI_NATIVE_INFO,"sceNetApctl"},
    {0x5bb34,0x5c510,0x24fe91a1,0x64c,WIFI_NATIVE_DISCONNECT,"sceNetApctl"},
    {0x5bb3c,0x5c514,0x5deac81b,0xb7c,WIFI_NATIVE_STATE,"sceNetApctl"},
};
static const browser_import_guard_t browser_guard_1={"sceHVUI_Module",1,1,0x5fa84,browser_libraries_1,sizeof(browser_libraries_1)/sizeof(browser_libraries_1[0]),browser_entries_1,sizeof(browser_entries_1)/sizeof(browser_entries_1[0])};
static const browser_import_library_t browser_libraries_2[]={
    {0x111048,0x111210,0x90011,0x10005,0x113268,0x110ee0,"sceNetApctl"},
};
static const browser_import_entry_t browser_entries_2[]={
    {0x110ee0,0x113268,0x5deac81b,0xb7c,WIFI_NATIVE_STATE,"sceNetApctl"},
};
static const browser_import_guard_t browser_guard_2={"sceHVNetfront_Module",1,1,0x122ce8,browser_libraries_2,sizeof(browser_libraries_2)/sizeof(browser_libraries_2[0]),browser_entries_2,sizeof(browser_entries_2)/sizeof(browser_entries_2[0])};
/* Sony HTTP calls APCTL independently of the browser/UI imports. */
static const browser_import_library_t browser_libraries_3[]={
    {0x17d48,0x17e98,0x10011,0x10005,0x1830c,0x179e8,"sceNetApctl"},
};
static const browser_import_entry_t browser_entries_3[]={
    {0x179e8,0x1830c,0x2befdf23,0x9e4,WIFI_NATIVE_INFO,"sceNetApctl"},
};
static const browser_import_guard_t browser_guard_3={"SceHttp_Library",1,2,0x18d1c,browser_libraries_3,1,browser_entries_3,1};
static const browser_import_guard_t *const browser_import_guards[]={&browser_guard_0,&browser_guard_1,&browser_guard_2,&browser_guard_3};
