#!/usr/bin/env python3
"""Build a pinned aemu fork with the ESP1000 game transport, without SDK writes."""
import argparse
from pathlib import Path
import shutil
import subprocess
import json
ROOT=Path(__file__).resolve().parents[1]
PIN='f6bc457eca56c4f08cad39aeac80fcc6dce6db6a'

def replace(path,old,new):
    text=path.read_text(encoding='latin1')
    if text.count(old)!=1:
        raise ValueError(f'{path}: expected one patch anchor, got {text.count(old)}')
    path.write_text(text.replace(old,new),encoding='latin1')

def prepare(source,output):
    actual=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
    if actual!=PIN:raise ValueError(f'Expected aemu {PIN}, got {actual}')
    if output.exists():raise ValueError('Use a fresh output directory')
    tree=output/'source'
    shutil.copytree(source,tree,ignore=shutil.ignore_patterns('.git','dist','*.o','*.elf','*.prx'))
    shutil.copytree(source/'procfw_sdk_prebuilt',output/'procfw')
    for path in tree.rglob('*'):
        if path.suffix in ('.c','.h','.S'):
            text=path.read_bytes()
            text=text.replace(b'ms0:/kd/',b'@ESP_KD@').replace(b'ms0:/seplugins/',b'ms0:/seplugins/esp-adhoc/')
            text=text.replace(b'@ESP_KD@',b'ms0:/seplugins/esp-adhoc/kd/')
            path.write_bytes(text)
    for makefile in tree.rglob('Makefile'):
        with makefile.open('a') as f:f.write('\nCFLAGS += -std=gnu99\n')
    for name in ['kd','seplugins','server']:(tree/'dist'/name).mkdir(parents=True,exist_ok=True)
    shutil.copy2(ROOT/'adhoc/aemu/esp_backend.c',tree/'pspnet_shims/esp_backend.c')
    shutil.copy2(ROOT/'adhoc/aemu/esp_power_imports.S',tree/'pspnet_shims/esp_power_imports.S')
    make=tree/'pspnet_shims/Makefile'
    replace(make,'OBJS = main.o','OBJS = main.o esp_backend.o esp_power_imports.o native_config.o native_config_abi.o lab_gateway.o')
    with make.open('a') as f:
        f.write(f'\nINCDIR += {ROOT} {ROOT}/common\nCFLAGS += -DVNET_USB_LAB -DNATIVE_HTTP_LOCAL_ONLY\nLIBS += -lpspkubridge -lpspsystemctrl_user -lpspnet -lpsputility -lpspnet_inet\n')
        for obj,src in [('native_config','virtual_net/app/native_config.c'),('native_config_abi','virtual_net/app/native_config_abi.c'),('lab_gateway','common/lab_gateway.c')]:
            f.write(f'{obj}.o: {ROOT}/{src}\n\t$(CC) $(CFLAGS) $(INCDIR:%=-I%) -c $< -o $@\n')
    replace(make,'include $(PSPSDK)/lib/build.mak','')
    with make.open('a') as f:f.write('\ninclude $(PSPSDK)/lib/build.mak\n')
    exports=tree/'pspnet_shims/exports.exp'
    functions=['espApctlInit','espApctlTerm','espApctlConnect','espApctlDisconnect','espApctlGetState','espApctlGetInfo','espLocalMac','espWlanSwitch']
    text='PSP_EXPORT_START(esp1000, 0x0011, 0x0001)\n'
    for i,name in enumerate(functions):text+=f'PSP_EXPORT_FUNC_NID({name}, {0x10+i:#x})\n'
    text+='PSP_EXPORT_END\nPSP_EXPORT_START(sceNetMiniUPnP, 0x0011, 0x0001)\n'
    for name,nid in [('espPortOpen',0xE66EEEC6),('espPortClose',0xB7DE11FB),('espMapsStart',1),('espMapsStop',2)]:text+=f'PSP_EXPORT_FUNC_NID({name}, {nid:#x})\n'
    text+='PSP_EXPORT_END\n'
    replace(exports,'PSP_END_EXPORTS',text+'\nPSP_END_EXPORTS')
    # Hook only user imports in this opt-in GAME plugin; never a VSH provider.
    hook='''
static void esp_boot_log(unsigned stage,int result)
{
    char line[48];const char prefix[]="ADHOC_PLUGIN ";unsigned used=sizeof(prefix)-1;
    memcpy(line,prefix,used);static const char hex[]="0123456789abcdef";
    for(unsigned j=0;j<8;++j)line[used++]=hex[(stage>>(28-4*j))&15];
    line[used++]=' ';
    for(unsigned j=0;j<8;++j)line[used++]=hex[((unsigned)result>>(28-4*j))&15];
    line[used++]='\\n';
    int fd=sceIoOpen("ms0:/esp-adhoc-plugin.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd>=0){sceIoWrite(fd,line,used);sceIoClose(fd);}
}
static SceModule2 *esp_game_module;
int esp_game_switch(void){return 1;}
static void esp_patch_imports(SceModule2 *module)
{
    if(sceKernelGetModel()!=10 || (module->text_addr&0x80000000))return;
    static const struct {const char *library;unsigned nid,id;} entries[]={
        {"sceNetApctl",0xe2f91f9b,0x10},{"sceNetApctl",0xb3edd0ec,0x11},
        {"sceNetApctl",0xcfb957c6,0x12},{"sceNetApctl",0x24fe91a1,0x13},
        {"sceNetApctl",0x5deac81b,0x14},{"sceNetApctl",0x2befdf23,0x15},
        {"sceNet",0x0bf0a3ae,0x16},{"sceWlanDrv",0x0c622081,0x16},
        {"sceWlanDrv",0xd7763699,0x17}
    };
    hook_import_bynid((SceModule *)module,"sceWlanDrv",0xd7763699,esp_game_switch);
    int utility_hook=hook_import_bynid((SceModule *)module,"sceUtility",0x1579a159,utility_load_netmodule);
    if(!utility_hook)esp_boot_log(10,utility_hook);
    hook_import_bynid((SceModule *)module,"sceUtility",0x64d50c56,utility_unload_netmodule);
    hook_import_bynid((SceModule *)module,"sceUtility",0x2a2b3de0,utility_load_module);
    hook_import_bynid((SceModule *)module,"sceUtility",0xe49bfe92,utility_unload_module);
    if(((module->text_addr>=0x08800000 && module->text_addr<0x08900000) || !strcmp(module->modname,"ESP1000 Ad Hoc Peer Test")) && strcmp(module->modname,"opnssmp") && strcmp(module->modname,"pspnet_shims") && strncmp(module->modname,"sceNet",6))esp_game_module=module;
    /* The loader may resolve weak imports again when APCTL arrives. Rebind
     * existing clients, including the game which started before the shim. */
    if(!strcmp(module->modname,"pspnet_shims") || !strcmp(module->modname,"sceNetApctl_Library")) {
        const char *names[]={"sceNetAdhoc_Library","sceNetAdhocctl_Library","sceNetAdhocMatching_Library"};
        for(unsigned i=0;i<3;++i) {
            SceModule2 *client=(SceModule2 *)sceKernelFindModuleByName(names[i]);
            if(client)esp_patch_imports(client);
        }
        if(esp_game_module && esp_game_module!=module)esp_patch_imports(esp_game_module);
    }
    /* The shim calls original functions internally; never redirect itself. */
    if(!strcmp(module->modname,"pspnet_shims"))return;
    for(unsigned i=0;i<sizeof(entries)/sizeof(entries[0]);++i) {
        void *fn=(void *)sctrlHENFindFunction("pspnet_shims","esp1000",entries[i].id);
        if(fn)hook_import_bynid((SceModule *)module,(char *)entries[i].library,entries[i].nid,fn);
    }
}
'''
    atpro=tree/'atpro/main.c'
    replace(atpro,'// Online Module Start Patcher',hook+'\n// Online Module Start Patcher')
    replace(atpro,'int sysctrl_patcher_result = sysctrl_patcher(module);','int sysctrl_patcher_result = sysctrl_patcher(module);\n    esp_patch_imports(module);')
    replace(atpro,'onlinemode = sceWlanGetSwitchState();','onlinemode = sceKernelGetModel()==10 ? 1 : sceWlanGetSwitchState();')
    replace(atpro,'// Initialize Logfile','esp_boot_log(1,sceKernelInitKeyConfig());\n    // Initialize Logfile')
    replace(atpro,'printk("Kernel Loader Hook: %d\\n", result);','esp_boot_log(2,result);\n    printk("Kernel Loader Hook: %d\\n", result);')
    replace(atpro,'printk("User Loader Hook: %d\\n", result);','esp_boot_log(3,result);\n    printk("User Loader Hook: %d\\n", result);')
    replace(atpro,'sctrlHENPatchSyscall((void*)sctrlHENFindFunction("sceLoadExec", "LoadExecForUser", 0x4AC57943), cbdeny);','if(sceKernelGetModel()!=10)sctrlHENPatchSyscall((void*)sctrlHENFindFunction("sceLoadExec", "LoadExecForUser", 0x4AC57943), cbdeny);')
    replace(atpro,'scePowerLock(0);','if(sceKernelGetModel()!=10)scePowerLock(0);')
    replace(tree/'atpro/exports.exp','PSP_END_EXPORTS','PSP_EXPORT_START(esp1000_kernel, 0, 0x4001)\nPSP_EXPORT_FUNC_HASH(esp_game_switch)\nPSP_EXPORT_END\nPSP_END_EXPORTS')
    replace(tree/'atpro/exports.exp','PSP_EXPORT_FUNC_HASH(module_stop)','PSP_EXPORT_FUNC_HASH(module_stop)\nPSP_EXPORT_FUNC_HASH(module_reboot_before)')
    for name in ['esp_power.c','esp_power_policy.h','esp_syscon.S']:
        shutil.copy2(ROOT/'adhoc/aemu'/name,tree/'atpro'/name)
    replace(tree/'atpro/Makefile','OBJS = main.o','OBJS = main.o esp_power.o esp_syscon.o')
    with (tree/'atpro/Makefile').open('a') as f:f.write('\nLIBS += -lpspusb\n')
    replace(atpro,'// Module Stop Event','extern int espAdhocPowerRelease(void);\n// Module Stop Event')
    replace(atpro,'// Shutdown GUI','if(espAdhocPowerRelease()<0)return -1;\n\t// Shutdown GUI')
    replace(tree/'atpro/exports.exp','PSP_END_EXPORTS','PSP_EXPORT_START(esp1000_power, 0, 0x4001)\nPSP_EXPORT_FUNC_NID(espAdhocPowerAcquire, 0x1)\nPSP_EXPORT_FUNC_NID(espAdhocPowerRelease, 0x2)\nPSP_EXPORT_END\nPSP_END_EXPORTS')
    # Build/install only this fork under esp-wifi; no global aemu files replaced.
    # Runtime paths retain aemu's existing kd/seplugins layout in the test package.
    (output/'provenance.json').write_text(json.dumps({'upstream':'https://github.com/Kethen/aemu','commit':PIN,'license':'GPL-3.0-or-later'},indent=2)+'\n')
    return tree

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=ROOT/'third_party/aemu')
    parser.add_argument('--output',type=Path,default=ROOT/'artifacts/aemu-esp')
    parser.add_argument('--build',action='store_true')
    args=parser.parse_args();tree=prepare(args.source.resolve(),args.output.resolve())
    if args.build:
        for component in ['atpro','memab','pspnet_adhoc_auth','pspnet_adhoc','pspnet_adhocctl','pspnet_adhoc_matching','pspnet_shims']:
            subprocess.run(['make','-C',str(tree/component),'CONFIG_660=1','RELEASE=1'],check=True)
if __name__=='__main__':main()
