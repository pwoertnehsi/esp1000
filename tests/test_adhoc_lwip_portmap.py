"""Run the real service against the ESP-IDF port-map implementation we build."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AdhocLwipPortmapTest(unittest.TestCase):
    def test_vendor_mapping_recovery(self):
        vendor=Path(os.environ.get('IDF_PATH','/opt/esp-idf'))/'components/lwip/lwip/src/core/ipv4/ip4_napt.c'
        if not vendor.exists():self.skipTest('ESP-IDF lwIP sources unavailable')
        source=vendor.read_text()
        structure=source[source.index('struct ip_portmap_entry {'):]
        structure=structure[:structure.index('};')+2]
        functions=source[source.index('u8_t\nip_portmap_add('):source.index('#endif /* IP_NAPT_PORTMAP */',source.index('u8_t\nip_portmap_add('))]
        # Keep vendor functions verbatim except symbol names, alongside the real
        # service. Existing wrappers count API calls and inject resource failure.
        functions=functions.replace('ip_portmap_', 'vendor_ip_portmap_')
        structure=structure.replace('ip_portmap_', 'vendor_ip_portmap_')
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory)
            for name in ['freertos/FreeRTOS.h','lwip/udp.h','lwip/timeouts.h','lwip/lwip_napt.h','lwip/netif.h','esp_log.h']:
                path=tmp/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('#include "adhoc_map_shim.h"\n')
            header='''#include "adhoc_map_shim.h"
#define PP_HTONS(x) __builtin_bswap16((uint16_t)(x))
#define PP_NTOHS PP_HTONS
'''+structure+'''
static struct vendor_ip_portmap_entry vendor_entries[32];
static struct vendor_ip_portmap_entry *vendor_ip_portmap_table=vendor_entries;
static uint8_t vendor_ip_portmap_max=32;
'''+functions
            (tmp/'vendor.h').write_text(header)
            test=(ROOT/'tests/adhoc_map_test.c').read_text().replace('#include "../esp_link/main/adhoc_portmap.c"',f'#include "{ROOT}/esp_link/main/adhoc_portmap.c"\n#include "vendor.h"')
            test=test.replace('return !reject_add;', 'return !reject_add && vendor_ip_portmap_add(protocol,external,port,internal,target);')
            test=test.replace('++removed;return 1;', '++removed;return vendor_ip_portmap_remove(protocol,port);')
            # A duplicate API add in this vendor implementation consumes a second
            # slot. The service must prevent it, and cleanup must empty the table.
            test=test.replace('    puts("PASS:', '    assert(!vendor_ip_portmap_find_dest(17,PP_HTONS(10001),0x024da8c0));\n    for(unsigned i=0;i<32;++i)assert(!vendor_entries[i].valid);\n    puts("PASS:')
            (tmp/'test.c').write_text(test)
            exe=tmp/'test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(tmp),'-I'+str(ROOT/'tests'),'-I'+str(ROOT/'common'),str(tmp/'test.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
