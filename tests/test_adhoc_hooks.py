import ast
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AdhocHooksTest(unittest.TestCase):
    def test_high_address_homebrew_module_loader(self):
        tree=ast.parse((ROOT/'tools/prepare_aemu.py').read_text())
        hook=next(node.value.value for node in ast.walk(tree) if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='hook' for t in node.targets))
        prefix=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_O_APPEND 4
typedef struct {unsigned text_addr;char modname[32];} SceModule2;
typedef SceModule2 SceModule;
static int model=10,hooks;static int (*loader)(int);
int sceKernelGetModel(void){return model;}
int sceIoOpen(const char *p,int f,int m){(void)p;(void)f;(void)m;return -1;}
int sceIoWrite(int f,const void *p,unsigned n){(void)f;(void)p;return n;}
int sceIoClose(int f){(void)f;return 0;}
int utility_load_netmodule(int id){return id+40;}
int utility_unload_netmodule(int id){return id;}
int utility_load_module(int id){return id;}
int utility_unload_module(int id){return id;}
int hook_import_bynid(SceModule *m,char *lib,unsigned nid,void *fn){
    (void)m;++hooks;
    if(!strcmp(lib,"sceUtility") && nid==0x1579a159){loader=fn;return 0;}
    return -1;
}
void *sctrlHENFindFunction(const char *m,const char *lib,unsigned nid){(void)m;(void)lib;(void)nid;return NULL;}
void *sceKernelFindModuleByName(const char *name){(void)name;return NULL;}
'''
        suffix=r'''
int main(void){
    SceModule2 app={.text_addr=0x09a00000,.modname="ESP1000 Ad Hoc Peer Test"};
    esp_patch_imports(&app);
    assert(loader==utility_load_netmodule && loader(2)==42 && esp_game_module==&app);
    int before=hooks;model=1;loader=NULL;esp_patch_imports(&app);assert(hooks==before && !loader);
    model=10;app.text_addr=0x88000000;esp_patch_imports(&app);assert(hooks==before);
    puts("PASS: generated E1000 hooks route high-address homebrew utility loads; foreign model/kernel excluded");
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory);(path/'test.c').write_text(prefix+hook+suffix)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(ROOT/'common'),str(path/'test.c'),'-o',str(path/'test')],check=True)
            subprocess.run([str(path/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
