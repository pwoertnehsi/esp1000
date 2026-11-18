#include <pspkernel.h>
#include <psploadcore.h>
#include <kubridge.h>
#include <systemctrl.h>
#include "dns.h"
typedef int (*dns_fn)(const void *,const void *);
static dns_fn setter;
static unsigned char old_primary[4],old_secondary[4];
static unsigned owner_text;
static int changed;
static int resolve(dns_fn *get,dns_fn *set)
{
    SceModule m;
    if(kuKernelFindModuleByName("sceNetInet_Library",&m)<0 || m.text_size!=0x31d70
       || m.version[0]!=3 || m.version[1]!=1)return -1;
    unsigned g=sctrlHENFindFunction("sceNetInet_Library","sceNetInet_lib",0xac9d90a5);
    unsigned s=sctrlHENFindFunction("sceNetInet_Library","sceNetInet_lib",0xaaf4895a);
    if(g!=m.text_addr+0x25dc || s!=m.text_addr+0x2630)return -1;
    static const struct{unsigned offset,word;} anchors[]={
        {0x25dc,0x27bdfff0},{0x25e4,0xafb00000},{0x25e8,0x24060004},{0x25ec,0x00a08021},
        {0x2630,0x27bdfff0},{0x2634,0x00801821},{0x263c,0xafb00000},{0x2644,0x00a08021}
    };
    for(unsigned i=0;i<sizeof(anchors)/sizeof(*anchors);++i)
        if(*(volatile unsigned *)(m.text_addr+anchors[i].offset)!=anchors[i].word)return -1;
    if(changed && owner_text!=m.text_addr)return -1;
    owner_text=m.text_addr;*get=(void *)g;*set=(void *)s;return 0;
}
int browser_dns_configure(const unsigned char primary[4],const unsigned char secondary[4])
{
    if(!(primary[0]|primary[1]|primary[2]|primary[3]))return -1;
    dns_fn get,set;if(changed || resolve(&get,&set)<0)return -1;
    int r=get(old_primary,old_secondary);if(r)return r;
    setter=set;r=setter(primary,secondary);if(!r)changed=1;return r;
}
int browser_dns_restore(void)
{
    if(!changed)return 0;
    dns_fn get,set;if(resolve(&get,&set)<0 || set!=setter)return -1;
    int r=setter(old_primary,old_secondary);if(!r)changed=0;return r;
}
