import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_router_recovery import function
ROOT=Path(__file__).resolve().parents[1]
DRIVER=ROOT/'esp_link/components/tinyusb/src/portable/synopsys/dwc2/hcd_dwc2.c'

class Dwc2FifoLayoutTest(unittest.TestCase):
    def test_real_allocator(self):
        code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
typedef struct { uint32_t ghwcfg2,gdfifocfg,grxfsiz,gnptxfsiz,hptxfsiz; } dwc2_regs_t;
typedef struct { uint16_t otg_dfifo_depth; } dwc2_controller_t;
typedef union { uint32_t value; struct { unsigned unused:14,num_host_ch:4; }; } dwc2_ghwcfg2_t;
static dwc2_regs_t regs;
static dwc2_controller_t _dwc2_controller[1];
static bool dma;
#define DWC2_REG(rhport) ((void)(rhport),&regs)
#define TUSB_EPSIZE_BULK_HS 512
#define TUSB_EPSIZE_ISO_HS_MAX 1024
#define TUSB_EPSIZE_BULK_FS 64
#define GDFIFOCFG_EPINFOBASE_SHIFT 16
#define TU_ASSERT(condition,...) assert(condition)
static bool dma_host_enabled(dwc2_regs_t *r){(void)r;return dma;}
uint8_t dwc2_channel_count(const dwc2_regs_t *r){return ((r->ghwcfg2>>14)&15)+1;}
static uint32_t tu_u32_from_u16(uint16_t high,uint16_t low){return (uint32_t)high<<16|low;}
'''+function(DRIVER.read_text(),'dfifo_host_init')+r'''
static void check(unsigned depth,unsigned channels,bool highspeed,bool use_dma)
{
    memset(&regs,0,sizeof(regs));
    _dwc2_controller[0].otg_dfifo_depth=depth;
    regs.ghwcfg2=(channels-1)<<14;dma=use_dma;
    dfifo_host_init(0,highspeed);
    unsigned rx=regs.grxfsiz;
    unsigned np_start=regs.gnptxfsiz&65535,np_size=regs.gnptxfsiz>>16;
    unsigned p_start=regs.hptxfsiz&65535,p_size=regs.hptxfsiz>>16;
    unsigned top=depth-(use_dma?channels:0);
    assert(rx && np_size && p_size);
    assert(p_start>=rx);                 /* RX starts at hardware address 0. */
    assert(p_start+p_size<=np_start);    /* TX regions must not overlap. */
    assert(np_start+np_size==top);       /* TX stays below metadata/end. */
    assert(rx==2*((highspeed?1024:256)/4+2)+channels);
    assert((regs.gdfifocfg&65535)==top && (regs.gdfifocfg>>16)==top);
    if(depth==256 && channels==8 && !use_dma) {
        assert(rx==140 && p_start==140 && p_size==84 && np_start==224 && np_size==32);
    }
    /* Model simultaneous RX and both TX buffers with disjoint ownership. */
    unsigned char owners[4096]={0};
    for(unsigned i=0;i<rx;++i)owners[i]=1;
    for(unsigned i=p_start;i<p_start+p_size;++i){assert(!owners[i]);owners[i]=2;}
    for(unsigned i=np_start;i<np_start+np_size;++i){assert(!owners[i]);owners[i]=3;}
}
int main(void)
{
    check(256,8,false,false);
    for(unsigned channels=1;channels<=16;++channels) {
        check(256,channels,false,false);
        check(320,channels,false,true);
        check(4096,channels,true,false);
        check(4096,channels,true,true);
    }
    puts("PASS: actual DWC2 allocator keeps RX, periodic/nonperiodic TX and DMA metadata disjoint");
}
'''
        with tempfile.TemporaryDirectory() as directory:
            tmp=Path(directory);(tmp/'test.c').write_text(code)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(tmp/'test.c'),'-o',str(tmp/'test')],check=True)
            subprocess.run([str(tmp/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
