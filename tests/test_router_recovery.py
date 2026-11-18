import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]

def function(source,name):
    start=source.index('static void '+name+'(')
    brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        if source[end]=='{':depth+=1
        elif source[end]=='}':depth-=1
        end+=1
    return source[start:end]

class RouterRecoveryTest(unittest.TestCase):
    def test_real_link_transition(self):
        source=(ROOT/'esp_link/main/usb_router.c').read_text()
        code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#define configASSERT assert
static bool linked;
static int usb_netif,outgoing,napt=1,generation=1,paused=1,resets,clears;
static void (*timer)(void *);
static void sys_timeout(unsigned delay,void (*fn)(void *),void *arg)
{assert(delay==10000 && !arg && !timer);timer=fn;}
static void sys_untimeout(void (*fn)(void *),void *arg)
{assert(!arg);if(timer==fn)timer=NULL;}
static int ip_napt_enable_netif(int *netif,int enabled)
{assert(netif==&usb_netif);if(enabled && !napt)++generation;napt=enabled;return 1;}
static void adhoc_portmap_pause(void){paused=1;}
static void adhoc_portmap_resume(void){assert(napt);paused=0;}
static void adhoc_portmap_clear(void){++clears;}
static void netif_set_link_up(int *netif){assert(netif==&usb_netif && linked && napt && !paused);}
static void netif_set_link_down(int *netif){assert(netif==&usb_netif && !linked && paused);}
static void etharp_cleanup_netif(int *netif){assert(netif==&usb_netif);}
static void xQueueReset(int queue){(void)queue;++resets;}
'''+function(source,'expire_napt')+'\n'+function(source,'link_change')+r'''
int main(void)
{
    link_change(&usb_netif);assert(linked && napt && generation==1);
    link_change(NULL);assert(!linked && paused && timer && napt && resets==1);
    link_change(NULL);assert(timer && napt && generation==1);
    link_change(&usb_netif);assert(!timer && napt && generation==1);
    link_change(NULL);assert(timer && napt);
    void (*fire)(void *)=timer;timer=NULL;fire(NULL);
    assert(!napt && clears==1);
    link_change(&usb_netif);assert(linked && napt && generation==2);
    expire_napt(NULL);assert(napt && clears==1);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory);(path/'test.c').write_text(code)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(path/'test.c'),'-o',str(path/'test')],check=True)
            subprocess.run([str(path/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
