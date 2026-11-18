import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AdhocPowerTest(unittest.TestCase):
    def test_lease(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for name in ['pspkernel.h','pspsdk.h','pspinit.h','psploadcore.h','pspusb.h','psputilsforkernel.h','pspsysmem_kernel.h','systemctrl.h']:
                (tmp/name).write_text('#include "adhoc_power_shim.h"\n')
            exe=tmp/'power-test'
            subprocess.run(['cc','-std=gnu99','-Wall','-Wextra','-Werror','-no-pie','-fsanitize=address,undefined','-I'+str(tmp),'-I'+str(ROOT/'tests'),str(ROOT/'tests/adhoc_power_test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

    def test_vsh_lease(self):
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for name in ['pspkernel.h','pspsdk.h','pspinit.h','psploadcore.h','pspusb.h','psputilsforkernel.h','pspsysmem_kernel.h','systemctrl.h']:
                (tmp/name).write_text('#include "adhoc_power_shim.h"\n#define PSP_INIT_KEYCONFIG_VSH 0x100\ntypedef SceModule2 SceModule;\n')
            source=(ROOT/'tests/adhoc_power_test.c').read_text()
            source=source.replace('#include "../adhoc/aemu/esp_power.c"',f'#include "{ROOT}/wifi_plugin/accessory_power.c"')
            source=source.replace('context=0x100','context=REFUSED_CONTEXT').replace('context=0x200','context=0x100').replace('context=REFUSED_CONTEXT','context=0x200')
            source=source.replace('int main(void)\n{','int main(void)\n{\n    context=0x100;assert(wifi_power_can_stop());')
            source=source.replace('assert(espAdhocPowerAcquire()==0 && owned && boot_waits==1);','assert(espAdhocPowerAcquire()==0 && owned && boot_waits==1 && !wifi_power_can_stop());')
            source=source.replace('    puts(', '    assert(wifi_power_can_stop());\n    puts(')
            (tmp/'test.c').write_text(source)
            exe=tmp/'vsh-power-test'
            subprocess.run(['cc','-std=gnu99','-Wall','-Wextra','-Werror','-no-pie','-fsanitize=address,undefined','-I'+str(tmp),'-I'+str(ROOT/'tests'),str(tmp/'test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
