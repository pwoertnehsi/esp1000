#include <assert.h>
#include <stdio.h>
#include "../virtual_net/driver/usb_transport.c"
static unsigned clock_us;
static int call, fail_at, cancel_stuck, unregisters, foreign_active;
static int bus_running,bus_starts,bus_stops,already_at,bus_race;
static const char *foreign_driver;
int sceUsbGetDrvState(const char *name)
{return !strcmp(name,PSP_USBBUS_DRIVERNAME)?(bus_running?1:2):foreign_driver && !strcmp(name,foreign_driver)?1:2;}
static int step(void) { return ++call==fail_at ? -7 : 0; }
int sceUsbGetState(void) { return foreign_active?0x200:0; }
int sceUsbbdRegister(struct UsbDriver *d) { assert(d==&driver); return step(); }
int sceUsbbdUnregister(struct UsbDriver *d) { assert(d==&driver && !tx_busy && !rx_busy); ++unregisters; return 0; }
int sceUsbAccRegisterType(u16 t) { assert(t==3); return step(); }
int sceUsbAccUnregisterType(u16 t) { assert(t==3); return 0; }
int sceUsbStart(const char *name,int n,void *a) {
    (void)n;(void)a;if(step()<0)return -7;
    if(already_at==call)return (int)0x80243001u;
    if(!strcmp(name,PSP_USBBUS_DRIVERNAME)) {
        ++bus_starts;
        if(bus_running || bus_race){bus_running=1;return (int)0x80243001u;}
        bus_running=1;
    }
    return !strcmp(name,DRIVER)?on_start(0,NULL):0;
}
int sceUsbStop(const char *name,int n,void *a) {
    (void)n;(void)a;assert(!tx_busy && !rx_busy);
    if(!strcmp(name,PSP_USBBUS_DRIVERNAME)){++bus_stops;bus_running=0;}
    return 0;
}
int sceUsbActivate(u32 p) { assert(p==LINK_PID); return step(); }
int sceUsbDeactivate(u32 p) { assert(p==LINK_PID); return 0; }
int sceUsbbdReqCancelAll(struct UsbEndpoint *e) {
    if(cancel_stuck) return 0;
    if(e==&endpoints[1]) { tx_req.retcode=-1; tx_complete(&tx_req,0,0); }
    else { rx_req.retcode=-1; rx_complete(&rx_req,0,0); }
    return 0;
}
int sceUsbbdReqSend(struct UsbdDeviceReq *r) { assert(r==&tx_req);return 0; }
int sceUsbbdReqRecv(struct UsbdDeviceReq *r) { assert(r==&rx_req);return 0; }
void sceKernelDcacheWritebackRange(const void *p,unsigned n) { (void)p;(void)n; }
void sceKernelDcacheInvalidateRange(const void *p,unsigned n) { (void)p;(void)n; }
u32 sceKernelGetSystemTimeLow(void) { return clock_us; }
int sceKernelDelayThread(unsigned n) { clock_us+=n;return 0; }
static void host_frame(link_frame_t f) {
    assert(rx_busy);rx=f;rx_req.retcode=0;rx_req.recvsize=64;rx_complete(&rx_req,0,0);
}
int main(void) {
    unsigned char data[PACKET_MAX]={0};
    foreign_active=1;assert(vnet_usb_start()<0 && !registered && !call);
    foreign_active=0;
    for(int i=1;i<=6;++i) {
        call=0;fail_at=i;assert(vnet_usb_start()<0);assert(!vnet_usb_stop());
        assert(!registered && !types && !bus && !acc && !own && !active);
    }
    fail_at=0;assert(!vnet_usb_start());
    on_attach(1,NULL,NULL);vnet_usb_poll(data,sizeof(data));vnet_usb_poll(data,sizeof(data));
    assert(tx_busy && tx.type==LINK_HELLO && rx_busy && !vnet_usb_ready());
    tx_req.retcode=0;tx_req.recvsize=64;tx_complete(&tx_req,0,0);
    link_frame_t f;link_init(&f,LINK_HELLO,1,0,LINK_BROWSER_HELLO);host_frame(f);
    assert(!vnet_usb_poll(data,sizeof(data)) && vnet_usb_ready());
    vnet_usb_progress_t progress;vnet_usb_progress(&progress);
    assert(progress.tx_frames==1 && progress.rx_frames==1 && !progress.tx_busy && progress.rx_busy);
    clock_us+=2000000;vnet_usb_progress(&progress);
    assert(progress.tx_age_us==2000000 && progress.rx_age_us==2000000);
    link_init(&f,LINK_NET_STATUS,2,0,NULL);f.length=8;link_put_u32(f.payload,3);
    const unsigned char ip[4]={203,0,113,7};memcpy(f.payload+4,ip,4);host_frame(f);
    vnet_usb_poll(data,sizeof(data));unsigned char supplied[4];
    assert(vnet_usb_uplink(supplied)==3 && !memcmp(supplied,ip,4));
    unsigned char radio[48],radio_copy[48];for(unsigned i=0;i<48;++i)radio[i]=i;
    assert(!vnet_usb_wifi_info(radio_copy));
    link_init(&f,LINK_WIFI_INFO,3,0,NULL);f.length=47;memcpy(f.payload,radio,47);host_frame(f);
    vnet_usb_poll(data,sizeof(data));assert(!vnet_usb_wifi_info(radio_copy));
    f.length=48;memcpy(f.payload,radio,48);host_frame(f);vnet_usb_poll(data,sizeof(data));
    assert(vnet_usb_wifi_info(radio_copy) && !memcmp(radio,radio_copy,48));
    assert(vnet_usb_send(data,1514) && !vnet_usb_can_send());
    packet_link_t host={0};int result=0;
    while(packet_tx.total || tx_busy) {
        vnet_usb_poll(data,sizeof(data));
        if(tx_busy) { result=packet_link_receive(&host,&tx);tx_req.retcode=0;tx_req.recvsize=64;tx_complete(&tx_req,0,0); }
    }
    assert(result==1514 && !memcmp(host.data,data,1514));
    /* Host-to-device fragmentation must become exactly one complete packet. */
    packet_link_t sender={0};
    assert(packet_link_begin(&sender,host.data,1514,77));
    unsigned chunks=0;
    while(packet_link_next(&sender,&f,++chunks)) {
        host_frame(f);
        int received=vnet_usb_poll(data,sizeof(data));
        assert(received==(sender.total?0:1514));
    }
    assert(!memcmp(data,host.data,1514));
    /* Detach+reattach before worker polls must cancel old buffers and handshake. */
    on_detach(0,0,0);on_attach(1,NULL,NULL);
    vnet_usb_poll(data,sizeof(data));assert(!vnet_usb_ready() && !vnet_usb_uplink(supplied) && !vnet_usb_wifi_info(radio_copy));
    vnet_usb_poll(data,sizeof(data));assert(tx_busy && rx_busy);
    int before=unregisters;cancel_stuck=1;
    assert(vnet_usb_stop()<0 && registered && unregisters==before && (tx_busy || rx_busy));
    cancel_stuck=0;assert(!vnet_usb_stop() && !registered);
    /* Native-only capability must handshake separately and sends credentials
     * only as an explicit committed transaction, never via browser HELLO. */
    assert(!vnet_usb_native_mode(1));assert(!vnet_usb_start());
    assert(vnet_usb_native_mode(0)<0);
    on_attach(1,NULL,NULL);vnet_usb_poll(data,sizeof(data));vnet_usb_poll(data,sizeof(data));
    tx_req.retcode=0;tx_req.recvsize=64;tx_complete(&tx_req,0,0);
    link_init(&f,LINK_HELLO,1,0,LINK_NATIVE_HELLO);host_frame(f);vnet_usb_poll(data,sizeof(data));
    assert(vnet_usb_ready());
    native_wifi_profile_t credentials={.ssid_length=2,.key_length=8,.security=4,.ssid={'a','p'},.key={'1','2','3','4','5','6','7','8'}};
    assert(!vnet_usb_profile_begin(77,&credentials));
    assert(vnet_usb_scan_begin(78,32)<0);
    native_profile_receiver_t receiver={0};unsigned records=native_profile_frames(&credentials);
    for(unsigned i=0;i<records;++i) {
        vnet_usb_poll(data,sizeof(data));assert(tx_busy && tx.type==LINK_WIFI_PROFILE);
        assert(native_profile_receive(&receiver,&tx)==(i+1==records?2:1));
        tx_req.retcode=0;tx_req.recvsize=64;tx_complete(&tx_req,0,0);
        /* poll may already prepare the next frame after completing this one */
        if(i+1==records)vnet_usb_poll(data,sizeof(data));
    }
    assert(!memcmp(&receiver.profile,&credentials,sizeof(credentials)));
    assert(!profile_count && !profile.ssid_length && !profile.key_length);
    assert(vnet_usb_profile_begin(78,&credentials)<0); /* ACK is still pending */
    link_init(&f,LINK_WIFI_PROFILE_RESULT,20,76,NULL);f.length=8;f.payload[0]=1;host_frame(f);vnet_usb_poll(data,sizeof(data));
    assert(!vnet_usb_profile_status());
    f.reply_to=77;host_frame(f);vnet_usb_poll(data,sizeof(data));assert(vnet_usb_profile_status()==1);
    assert(!vnet_usb_scan_begin(79,32));vnet_usb_poll(data,sizeof(data));assert(tx_busy && tx.type==LINK_WIFI_SCAN_REQUEST);
    tx_req.retcode=0;tx_req.recvsize=64;tx_complete(&tx_req,0,0);vnet_usb_poll(data,sizeof(data));
    native_scan_row_t ap={.ssid_length=2,.ssid={'a','p'},.channel=6,.rssi=-47,.auth=NATIVE_AUTH_WPA2,.cipher=NATIVE_CIPHER_AES};
    assert(native_scan_encode_row(&f,30,79,0,&ap));host_frame(f);vnet_usb_poll(data,sizeof(data));
    assert(native_scan_encode_done(&f,31,79,1,0));host_frame(f);vnet_usb_poll(data,sizeof(data));
    native_scan_session_t snapshot;assert(vnet_usb_scan_snapshot(&snapshot)==1 && snapshot.count==1);
    assert(!vnet_usb_stop());
    /* VSH keeps a started but inactive base bus. Borrow it without either
     * starting it twice or stopping somebody else's bus during cleanup. */
    bus_running=1;int starts_before=bus_starts,stops_before=bus_stops;
    assert(!vnet_usb_start() && !bus && active && own && acc);
    assert(!vnet_usb_stop() && bus_running && bus_starts==starts_before && bus_stops==stops_before);
    const char *blocked[]={"USBStor_Driver","USBCamDriver","USBAccBaseDriver"};
    for(unsigned i=0;i<3;++i){foreign_driver=blocked[i];int calls_before=call;assert(vnet_usb_start()<0 && call==calls_before && bus_running);}
    foreign_driver=NULL;bus_running=0;bus_race=1;
    assert(!vnet_usb_start() && bus_running && !bus);assert(!vnet_usb_stop() && bus_running);
    bus_race=0;bus_running=0;call=0;already_at=4;
    assert((unsigned)vnet_usb_start()==0x80243001u && start_step==4);
    assert(!vnet_usb_stop() && !bus_running && !registered && !acc);
    already_at=0;
    puts("PASS: USB ownership, six startup failures, handshake, full packet send, reconnect reset, retained cancellation buffers");
}
