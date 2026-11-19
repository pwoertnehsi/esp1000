from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
class XmbMenuTest(unittest.TestCase):
    def test_actual_runtime(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            shim='''#ifndef XMB_SHIM
#define XMB_SHIM
#include <stdint.h>
typedef int SceUID;
typedef struct {char modname[28];unsigned char version[2],nsegment;unsigned attribute;int modid;unsigned text_addr,text_size,segmentaddr[4],segmentsize[4];} SceModule;
typedef int (*STMOD_HANDLER)(SceModule *);
unsigned pspSdkSetK1(unsigned);
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int);
void sceKernelDcacheWritebackInvalidateAll(void);
void sceKernelIcacheInvalidateAll(void);
SceModule *sceKernelFindModuleByName(const char *);
SceModule *sceKernelFindModuleByUID(int);
SceModule *sceKernelFindModuleByAddress(unsigned);
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER);
#endif
'''
            (tmp/'shim.h').write_text(shim)
            for name in ['pspkernel.h','psploadcore.h','pspsdk.h','systemctrl.h']:
                (tmp/name).write_text('#include "shim.h"\n')
            exe=tmp/'test'
            for base in ['0x08800000u','0x0a040f00u']:
                subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-int-to-pointer-cast',
                    '-fsanitize=address,undefined','-DVSH_TEST_BASE='+base,'-I',str(tmp),
                    str(ROOT/'tests/xmb_menu_runtime_test.c'),'-o',str(exe)],check=True)
                subprocess.run([str(exe)],check=True,timeout=10,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_settings_navigation(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            (tmp/'shim.h').write_text('''#ifndef UI_SHIM
#define UI_SHIM
#define PSP_CTRL_UP 0x10
#define PSP_CTRL_DOWN 0x40
#define PSP_CTRL_CROSS 0x4000
#define PSP_CTRL_CIRCLE 0x2000
typedef struct {unsigned Buttons;} SceCtrlData;
void pspDebugScreenClear(void);
int pspDebugScreenPrintf(const char *,...);
int sceCtrlPeekBufferPositive(SceCtrlData *,int);
int sceDisplayWaitVblankStart(void);
int sceKernelDelayThreadCB(unsigned);
#endif
''')
            for name in ['pspkernel.h','pspctrl.h','pspdebug.h','pspdisplay.h']:
                (tmp/name).write_text('#include "shim.h"\n')
            exe=tmp/'test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-I',str(tmp),str(ROOT/'tests/network_menu_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=10,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_catalog_dispatch_coverage(self):
        import json
        catalog=json.loads((ROOT/'analysis/xmb_menu_catalog.json').read_text())
        rows=catalog['rows'] if isinstance(catalog,dict) else catalog
        assertions=[]
        for row in rows:
            op='!=' if row['restore'] else '=='
            assertions.append(f"assert(wifi_menu_command({row['action']},{row['argument']}){op}0);")
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            source=tmp/'dispatch.c'
            source.write_text('#include <assert.h>\n#include "wifi_plugin/menu_control.h"\nint main(void){'+''.join(assertions)+'return 0;}')
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT),str(source),'-o',str(tmp/'test')],check=True)
            subprocess.run([str(tmp/'test')],check=True)

    def test_native_constructor_integrity(self):
        """Replay native SHA-1/default-table selection against actual C output."""
        import ctypes
        import hashlib
        import json
        import struct
        catalog=json.loads((ROOT/'analysis/xmb_menu_catalog.json').read_text())
        text=bytearray(349888)
        table=b''.join(struct.pack('<4I',r['category'],r['item'],r['mask'],r['region']) for r in catalog['rows'])
        text[0x54b20:0x54b20+560]=table
        struct.pack_into('<I',text,0x20c20,0x50620029)
        for addr,word in [(0x20c1c,0x2402000a),(0x20c24,0x8e440a6c),(0x20c28,0x44806000),(0x20cc8,0x8c850334),(0x20cd0,0x24a5fffe)]:
            struct.pack_into('<I',text,addr,word)
        for resource in catalog['resources']:
            struct.pack_into('<5I',text,resource['offset'],*resource['words'])
        struct.pack_into('<I',text,0x16a74,0xafb3000c)
        for address,word in catalog['anchors']:struct.pack_into('<I',text,address,word)
        base=0x0a040f00
        for offset in [0x16a4c,0x308b4]:struct.pack_into('<I',text,offset,0x0c000000|((base+0x16a70)>>2))
        data=bytearray(64);data[16:36]=hashlib.sha1(table).digest()
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory);source=tmp/'core.c';lib=tmp/'core.so'
            source.write_text('''#include "wifi_plugin/menu_patch.h"
int apply(uint32_t *text,unsigned char *data) {
 uint32_t cb[2]={0x0880a8a4,0x0880a894};uint32_t trampoline[4];memcpy(trampoline,menu_trampoline_empty,16);
 int r=menu_patch_prepare(text,349888,0x0a040f00,cb,data,64,trampoline,0x09000400u);
 if(!r){menu_patch_apply(text,data,cb,trampoline,0x0a040f00u);r=menu_patch_verify(text,data,cb,trampoline,0x0a040f00u);}
 return r;
}
''')
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC','-I',str(ROOT),str(source),'-o',str(lib)],check=True)
            core=ctypes.CDLL(str(lib));core.apply.argtypes=[ctypes.c_void_p,ctypes.c_void_p];core.apply.restype=ctypes.c_int
            # A reference digest mismatch or unrelated table edit is read-only refusal.
            damaged=bytearray(data);damaged[16]^=1
            before=bytes(text);bad_buf=(ctypes.c_ubyte*len(damaged)).from_buffer(damaged)
            text_buf=(ctypes.c_ubyte*len(text)).from_buffer(text)
            self.assertLess(core.apply(text_buf,bad_buf),0);self.assertEqual(bytes(text),before)
            text[0x54b20]^=1
            data_buf=(ctypes.c_ubyte*len(data)).from_buffer(data)
            self.assertLess(core.apply(text_buf,data_buf),0);text[0x54b20]^=1
            # The original BEQL model==10 reaches native SetColumnCount(count-2).
            def rendered_columns(branch,model):
                return 6 if branch==0x50620029 and model==10 else 8
            self.assertEqual(rendered_columns(struct.unpack_from('<I',text,0x20c20)[0],10),6)
            self.assertEqual(core.apply(text_buf,data_buf),2)
            self.assertEqual(rendered_columns(struct.unpack_from('<I',text,0x20c20)[0],10),8)
            self.assertEqual(rendered_columns(struct.unpack_from('<I',text,0x20c20)[0],1),8)
            modified=bytes(text[0x54b20:0x54b20+560])
            # Native constructor restores table on digest mismatch. The old patch
            # fails this check; v0.7's actual output preserves its patched table.
            self.assertNotEqual(hashlib.sha1(modified).digest(),hashlib.sha1(table).digest())
            selected=modified if hashlib.sha1(modified).digest()==bytes(data[16:36]) else table
            self.assertEqual(selected,modified)
            for i,row in enumerate(catalog['rows']):
                expected=row['mask']|(0x400 if row['restore'] else 0)
                self.assertEqual(struct.unpack_from('<I',selected,i*16+8)[0],expected)

    def test_actual_user_bridge(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            (tmp/'pspkernel.h').write_text('''#define PSP_MODULE_INFO(...)
#define PSP_MAIN_THREAD_ATTR(...)
#define PSP_THREAD_ATTR_USER 0x80000000
typedef unsigned SceSize;
typedef int SceUID;
#ifndef NULL
#define NULL ((void *)0)
#endif
int sceKernelCreateThread(const char *,int (*)(SceSize,void *),int,int,int,void *);
int sceKernelStartThread(int,unsigned,void *);
int sceKernelDeleteThread(int);
int sceKernelWaitThreadEnd(int,unsigned *);
int sceKernelDelayThread(unsigned);
''')
            (tmp/'pspiofilemgr.h').write_text('#define PSP_O_WRONLY 1\n#define PSP_O_CREAT 2\n#define PSP_O_APPEND 4\nint sceIoOpen(const char *,int,int);\nint sceIoWrite(int,const void *,unsigned);\nint sceIoClose(int);\n')
            exe=tmp/'bridge'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-pointer-to-int-cast',
                '-Wno-int-to-pointer-cast','-fno-pie','-no-pie','-fsanitize=address,undefined',
                '-I',str(tmp),str(ROOT/'tests/wifi_menu_bridge_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=10,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
