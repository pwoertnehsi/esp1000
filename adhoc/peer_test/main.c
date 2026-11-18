#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <pspnet.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <stdio.h>
#include <string.h>
PSP_MODULE_INFO("ESP1000 Ad Hoc Peer Test",0,0,1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(512);
static volatile int quit;
static int exit_cb(int a,int b,void *c){(void)a;(void)b;(void)c;quit=1;return 0;}
static int callbacks(SceSize n,void *p)
{(void)n;(void)p;int cb=sceKernelCreateCallback("exit",exit_cb,NULL);sceKernelRegisterExitCallback(cb);sceKernelSleepThreadCB();return 0;}
static int step(const char *name,int result)
{pspDebugScreenPrintf("%s: %08x\n",name,result);return result;}
int main(void)
{
    pspDebugScreenInit();sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    int cb=sceKernelCreateThread("callbacks",callbacks,0x10,4096,PSP_THREAD_ATTR_USER,NULL);if(cb>=0)sceKernelStartThread(cb,0,NULL);
    pspDebugScreenPrintf("ESP1000 / PPSSPP Ad Hoc peer test\n\nRun this same EBOOT on both devices.\nServer: set in esp-adhoc/server.txt\nPPSSPP: WLAN + built-in server ON,\nserver = PC LAN IP, offset = 10000,\nrelay / ENet / UPnP OFF for this LAN test.\n\nCROSS starts; HOME exits.\n");
    while(!quit){SceCtrlData pad;sceCtrlReadBufferPositive(&pad,1);if(pad.Buttons&PSP_CTRL_CROSS)break;sceKernelDelayThread(20000);}
    if(quit){sceKernelExitGame();return 0;}
    int socket=-1,ctl=0,adhoc=0,net=0;
    if(step("NET module",sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON))<0)goto done;
    if(step("Ad Hoc modules",sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC))<0)goto done;
    if(step("NET init",sceNetInit(256*1024,0x30,4096,0x30,4096))<0)goto done;
    net=1;
    if(step("Ad Hoc init",sceNetAdhocInit())<0)goto done;
    adhoc=1;
    struct productStruct product={.unknown=0,.product={'E','S','P','1','0','0','0','0','1'},.unk={0,0,0}};
    if(step("Control init",sceNetAdhocctlInit(16384,0x30,&product))<0)goto done;
    ctl=1;
    if(step("Join ESP1000",sceNetAdhocctlConnect("ESP1000"))<0)goto done;
    unsigned began=sceKernelGetSystemTimeLow();int state=0;
    while(!quit && sceKernelGetSystemTimeLow()-began<20000000u){if(sceNetAdhocctlGetState(&state)<0)goto done;if(state==1)break;sceKernelDelayThread(20000);}
    if(state!=1){step("Group timeout",-1);goto done;}
    unsigned char mac[6],broadcast[6];sceNetGetLocalEtherAddr(mac);memset(broadcast,255,6);
    socket=step("PDP socket",sceNetAdhocPdpCreate(mac,3090,8192,0));if(socket<1)goto done;
    pspDebugScreenPrintf("Group joined. Waiting for peer packets...\n");
    unsigned sent=0,received=0,last=0;
    while(!quit) {
        unsigned now=sceKernelGetSystemTimeLow();
        if(now-last>=1000000u) {
            char text[64];int n=snprintf(text,sizeof(text),"ESP1000 peer ping %u",++sent);
            int r=sceNetAdhocPdpSend(socket,broadcast,3090,text,n+1,100000,1);
            pspDebugScreenPrintf("TX %u result=%08x RX=%u\n",sent,r,received);last=now;
        }
        unsigned char peer[6];unsigned short port;char text[96];int bytes=sizeof(text)-1;
        int r=sceNetAdhocPdpRecv(socket,peer,&port,text,&bytes,0,1);
        if(!r && bytes>0 && bytes<(int)sizeof(text)) {
            text[bytes]=0;++received;pspDebugScreenPrintf("RX #%u port %u: %s\n",received,port,text);
        }
        sceKernelDelayThread(10000);
    }
done:
    if(socket>0)step("PDP close",sceNetAdhocPdpDelete(socket,0));
    if(ctl){step("Disconnect",sceNetAdhocctlDisconnect());step("Control term",sceNetAdhocctlTerm());}
    if(adhoc)step("Ad Hoc term",sceNetAdhocTerm());
    if(net)step("NET term",sceNetTerm());
    pspDebugScreenPrintf("Test ended. HOME exits.\n");while(!quit)sceKernelDelayThread(20000);
    sceKernelExitGame();return 0;
}
