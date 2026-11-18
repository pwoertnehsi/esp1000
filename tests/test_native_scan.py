from pathlib import Path
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class NativeScanTest(unittest.TestCase):
    def test_wire(self):
        with tempfile.TemporaryDirectory() as d:
            exe=Path(d)/'scan'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(ROOT/'tests/native_scan_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_profiles(self):
        with tempfile.TemporaryDirectory() as d:
            exe=Path(d)/'profile'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(ROOT/'tests/native_profile_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_native_hooks(self):
        with tempfile.TemporaryDirectory() as d:
            tmp=Path(d)
            (tmp/'shim.h').write_text("""#ifndef NATIVE_TEST_SHIM
#define NATIVE_TEST_SHIM
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
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER);
unsigned sctrlHENFindFunction(const char *,const char *,unsigned);
#endif
""")
            for name in ['pspkernel.h','psploadcore.h','pspsdk.h','systemctrl.h']:
                (tmp/name).write_text('#include "shim.h"\n')
            exe=tmp/'runtime'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-int-to-pointer-cast','-fsanitize=address,undefined','-I',str(tmp),str(ROOT/'tests/native_runtime_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_native_adapter(self):
        with tempfile.TemporaryDirectory() as d:
            tmp=Path(d)
            for name in ['pspkernel.h','psploadcore.h','pspmodulemgr.h','psputility_usbmodules.h','psputility_netparam.h','psputility_netmodules.h','pspnet_apctl.h','kubridge.h','pspiofilemgr.h','pspusb.h']:
                (tmp/name).write_text('#include "native_adapter_shim.h"\n')
            exe=tmp/'adapter'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-fno-pie','-no-pie','-fsanitize=address,undefined','-I',str(tmp),'-I',str(ROOT/'tests'),str(ROOT/'tests/native_adapter_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_native_session(self):
        with tempfile.TemporaryDirectory() as d:
            tmp=Path(d)
            for name in ['pspkernel.h','pspmodulemgr.h','pspnet.h','pspnet_inet.h','pspnet_resolver.h','pspnet_apctl.h','kubridge.h']:
                (tmp/name).write_text('#include "native_session_shim.h"\n')
            exe=tmp/'session'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-fsanitize=address,undefined','-I',str(tmp),'-I',str(ROOT/'tests'),str(ROOT/'tests/native_session_test.c'),'-o',str(exe)],check=True)
            for mode in [0,1,2,3,4,5,7]:
                subprocess.run([str(exe),str(mode)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_native_usb_owner(self):
        with tempfile.TemporaryDirectory() as d:
            tmp=Path(d)
            for name in ['pspkernel.h','psploadcore.h']:
                (tmp/name).write_text('#include "native_usb_shim.h"\n')
            exe=tmp/'usb-owner'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-int-to-pointer-cast','-fsanitize=address,undefined','-I',str(tmp),'-I',str(ROOT/'tests'),'-I',str(ROOT/'common'),str(ROOT/'tests/native_usb_test.c'),'-o',str(exe)],check=True)
            for mode in range(8):
                subprocess.run([str(exe),str(mode)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_native_guard_catalog(self):
        import re
        import sys
        sys.path.insert(0,str(ROOT/'tools'))
        from inspect_prx import Prx
        module=Prx(ROOT/'modules/netconf_plugin.prx',{})
        api=Prx(ROOT/'modules/pspnet_apctl.prx',{})
        exports={int(e['nid'],16):int(e['address'],16) for lib in api.report()['exports'] for e in lib['entries']}
        metadata=(ROOT/'wifi_plugin/native_import_guards.h').read_text()
        for fields in re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+),"([^"]+)"\}',metadata):
            offset,name,nids,stubs,flags,counts=[int(x,16) for x in fields[:6]]
            self.assertEqual([module.word(offset+i) for i in (0,12,16,4,8)],[name,nids,stubs,flags,counts])
            self.assertEqual(module.string(name),fields[6])
            self.assertEqual((flags>>16)&0xffff,9)
        runtime=(ROOT/'wifi_plugin/native_runtime.c').read_text()
        rows=re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+),WIFI_NATIVE_[A-Z]+,(0x[0-9a-f]+),(0x[0-9a-f]+),"',runtime)
        self.assertEqual(len(rows),11)
        for stub,nid,api_offset,nid_offset in rows:
            stub,nid,api_offset,nid_offset=map(lambda x:int(x,16),(stub,nid,api_offset,nid_offset))
            self.assertEqual(module.word(nid_offset),nid)
            self.assertEqual(exports[nid],api_offset)
            self.assertEqual([module.word(stub),module.word(stub+4)],[0x03e00008,0])
