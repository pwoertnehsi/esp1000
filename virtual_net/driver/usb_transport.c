#include <pspkernel.h>
#include <pspusb.h>
#include <pspusbbus.h>
#include <pspsdk.h>
#include <string.h>
#include "packet_link.h"
#include "usb_transport.h"
extern int sceUsbAccRegisterType(u16);
extern int sceUsbAccUnregisterType(u16);
#define DRIVER "PSPWifiLink"
#define ACC_TYPES 3
static struct UsbEndpoint endpoints[3] = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
static struct UsbInterface interface = {-1, 0, 1};
static struct UsbData descriptors[2] __attribute__((aligned(64)));
static struct UsbDriver driver;
static link_frame_t tx __attribute__((aligned(64)));
static link_frame_t rx __attribute__((aligned(64)));
static struct UsbdDeviceReq tx_req, rx_req;
static volatile int attached, speed;
static volatile int epoch_reset;
static volatile int tx_busy, rx_busy, tx_done, rx_done;


static int native_mode,scan_pending;
static native_wifi_profile_t profile;
static unsigned profile_request,profile_index,profile_count;
static int profile_result;
static native_scan_session_t scan;
static link_frame_t scan_request;
static unsigned start_step;
static unsigned seq;
static unsigned sent_frames,received_frames,last_sent,last_received;

static packet_link_t packet_tx, packet_rx;
static int registered, types, bus, acc, own, active, hello_pending, ready;
static unsigned packet_id, usb_errors, error_stage, error_bytes;
static int error_code;
static unsigned uplink;
static unsigned char wifi_info[48];
static int wifi_info_valid;
int vnet_usb_wifi_info(unsigned char info[48]) {memcpy(info,wifi_info,48);return wifi_info_valid;}
static unsigned char remote_ip[4];
unsigned vnet_usb_uplink(unsigned char ip[4]) { memcpy(ip,remote_ip,4); return uplink; }
static void usb_error(unsigned stage, int code, unsigned bytes)
{ ++usb_errors; error_stage=stage; error_code=code; error_bytes=bytes; }
void vnet_usb_last_error(unsigned *stage, int *code, unsigned *bytes)
{ *stage=error_stage; *code=error_code; *bytes=error_bytes; }
/* Callbacks only publish state. All IO and buffer reuse happens in worker. */
static int tx_complete(struct UsbdDeviceReq *req, int a, int b)
{
    (void)req; (void)a; (void)b;
    tx_done = 1;
    return 0;
}

static int rx_complete(struct UsbdDeviceReq *req, int a, int b)
{
    (void)req; (void)a; (void)b;
    rx_done = 1;
    return 0;
}

static int on_attach(int value, void *a, void *b)
{
    (void)a; (void)b;
    speed = value;
    attached = 1;
    epoch_reset = 1;
    return 0;
}

static int on_detach(int a, int b, int c)
{
    (void)a; (void)b; (void)c;
    attached = 0;
    epoch_reset = 1;
    return 0;
}

static int on_control(int a, int b, struct DeviceRequest *request)
{
    (void)a; (void)b; (void)request;
    return -1; /* Accessory requests belong to USBAccBaseDriver. */
}

static void make_descriptors(int index, int packet_size)
{
    struct UsbData *d = &descriptors[index];
    const struct DeviceDescriptor device = {
        .bLength = 18, .bDescriptorType = 1, .bcdUSB = 0x0200,
        .bMaxPacketSize = 64, .bcdDevice = 0x0100, .bNumConfigurations = 1,
    };
    const struct ConfigDescriptor config = {
        .bLength = 9, .bDescriptorType = 2, .wTotalLength = 32,
        .bNumInterfaces = 1, .bConfigurationValue = 1, .bmAttributes = 0xc0,
    };
    const struct InterfaceDescriptor itf = {
        .bLength = 9, .bDescriptorType = 4, .bNumEndpoints = 2,
        .bInterfaceClass = 0xff, .bInterfaceSubClass = LINK_SUBCLASS,
        .bInterfaceProtocol = LINK_PROTOCOL,
    };
    const struct EndpointDescriptor in = {
        .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x81,
        .bmAttributes = 2, .wMaxPacketSize = packet_size,
    };
    const struct EndpointDescriptor out = {
        .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x02,
        .bmAttributes = 2, .wMaxPacketSize = packet_size,
    };
    memset(d, 0, sizeof(*d));
    memcpy(d->devdesc, &device, sizeof(device));
    d->config.pconfdesc = &d->confdesc;
    d->config.pinterfaces = &d->interfaces;
    d->config.pinterdesc = &d->interdesc;
    d->config.pendp = d->endp;
    memcpy(d->confdesc.desc, &config, sizeof(config));
    d->confdesc.pinterfaces = &d->interfaces;
    d->interfaces.pinterdesc[0] = &d->interdesc;
    d->interfaces.intcount = 1;
    memcpy(d->interdesc.desc, &itf, sizeof(itf));
    d->interdesc.pendp = d->endp;
    memcpy(d->endp[0].desc, &in, sizeof(in));
    memcpy(d->endp[1].desc, &out, sizeof(out));
}

static int on_start(int size, void *args)
{
    (void)size; (void)args;
    make_descriptors(0, 512);
    make_descriptors(1, 64);
    driver.devp_hi = descriptors[0].devdesc;
    driver.confp_hi = &descriptors[0].config;
    driver.devp = descriptors[1].devdesc;
    driver.confp = &descriptors[1].config;
    return 0;
}

static int on_stop(int size, void *args)
{
    (void)size; (void)args;
    attached = 0;
    epoch_reset = 1;
    return 0;
}

static struct UsbDriver driver = {
    .name = DRIVER, .endpoints = 3, .endp = endpoints, .intp = &interface,
    .recvctl = on_control, .attach = on_attach, .detach = on_detach,
    .start_func = on_start, .stop_func = on_stop,
};


int vnet_usb_native_mode(int enabled)
{
    if(registered || active)return -1;
    native_mode=enabled!=0;scan_pending=0;memset(&scan,0,sizeof(scan));
    native_profile_clear(&profile,sizeof(profile));profile_request=profile_index=profile_count=0;profile_result=0;return 0;
}
int vnet_usb_profile_begin(unsigned request,const native_wifi_profile_t *input)
{
    if(!native_mode || !vnet_usb_ready() || !request || scan.busy || profile_request || !native_profile_valid(input))return -1;
    profile=*input;profile_request=request;profile_index=0;profile_count=native_profile_frames(input);profile_result=0;return 0;
}
int vnet_usb_profile_status(void){return profile_result;}
int vnet_usb_scan_begin(unsigned request,unsigned limit)
{
    if(!native_mode || profile_request || !vnet_usb_ready() || native_scan_begin(&scan,request,limit))return -1;
    link_init(&scan_request,LINK_WIFI_SCAN_REQUEST,request,0,NULL);
    scan_request.length=4;scan_request.payload[0]=NATIVE_SCAN_VERSION;scan_request.payload[1]=limit;
    scan_pending=1;return 0;
}
int vnet_usb_scan_snapshot(native_scan_session_t *out)
{*out=scan;return scan.complete?1:scan.error?-1:0;}
static int foreign_protocol_started(void)
{
    static const char *names[]={"USBStor_Driver","USBStorBoot_Driver","USBStorFlash_Driver",
        "USBStorMLN_Driver","USBPSPCommunicationDriver","USBCamDriver","USBCamMicDriver","USBGpsDriver"};
    for(unsigned i=0;i<sizeof(names)/sizeof(*names);++i)
        if(sceUsbGetDrvState(names[i])==1)return 1;
    return 0;
}
unsigned vnet_usb_start_step(void){return start_step;}
int vnet_usb_start(void)
{
    start_step=0;
    /* A started base bus is shared infrastructure in VSH. An activated bus or
     * started foreign function driver is not available to this service. */
    if((sceUsbGetState()&PSP_USB_ACTIVATED) || foreign_protocol_started() ||
        sceUsbGetDrvState("USBAccBaseDriver")==1)return -1;
#define STEP(number,expr,flag) do { start_step=number;int r=(expr);if(r<0)return r;flag=1; } while(0)
    STEP(1,sceUsbbdRegister(&driver),registered);
    STEP(2,sceUsbAccRegisterType(ACC_TYPES),types);
    start_step=3;
    int bus_state=sceUsbGetDrvState(PSP_USBBUS_DRIVERNAME);
    if(bus_state<0)return bus_state;
    if(bus_state!=1) {
        int r=sceUsbStart(PSP_USBBUS_DRIVERNAME,0,NULL);
        if(r>=0)bus=1; /* Only stop the bus if this instance actually started it. */
        else if((unsigned)r!=0x80243001u || sceUsbGetDrvState(PSP_USBBUS_DRIVERNAME)!=1 ||
            (sceUsbGetState()&PSP_USB_ACTIVATED) || foreign_protocol_started())return r;
    }
    STEP(4,sceUsbStart("USBAccBaseDriver",0,NULL),acc);
    STEP(5,sceUsbStart(DRIVER,0,NULL),own);
    start_step=6;
    if((sceUsbGetState()&PSP_USB_ACTIVATED) || foreign_protocol_started())return -1;
    STEP(6,sceUsbActivate(LINK_PID),active);
    start_step=0;return 0;
#undef STEP
}
unsigned vnet_usb_errors(void) { return usb_errors; }
int vnet_usb_ready(void) { return attached && ready; }
void vnet_usb_progress(vnet_usb_progress_t *p)
{
    unsigned now=sceKernelGetSystemTimeLow();
    p->tx_busy=tx_busy;p->rx_busy=rx_busy;
    p->packet_total=packet_tx.total;p->packet_offset=packet_tx.offset;
    p->tx_frames=sent_frames;p->rx_frames=received_frames;
    p->tx_age_us=sent_frames?now-last_sent:0;
    p->rx_age_us=received_frames?now-last_received:0;
}
int vnet_usb_can_send(void) { return vnet_usb_ready() && !packet_tx.total; }
int vnet_usb_send(const void *data,unsigned n)
{
    return vnet_usb_can_send() && packet_link_begin(&packet_tx,data,n,++packet_id);
}
/* Called by the packet worker only. Callbacks merely publish completion. */
int vnet_usb_poll(void *data,unsigned capacity)
{
    int result=0;
    if (!attached || epoch_reset) {
        if(scan.busy)native_scan_abort(&scan,NATIVE_SCAN_NOT_READY);
        scan_pending=0;
        if(profile_request)profile_result=-1;
        profile_request=profile_count=0;native_profile_clear(&profile,sizeof(profile));
        ready=0; uplink=0; wifi_info_valid=0; memset(wifi_info,0,48); memset(remote_ip,0,4); hello_pending=1; packet_tx.total=packet_rx.total=0;
        if(tx_busy) sceUsbbdReqCancelAll(&endpoints[1]);
        if(rx_busy) sceUsbbdReqCancelAll(&endpoints[2]);
    }
    if(tx_busy && tx_done) {
        tx_busy=tx_done=0;
        if(!tx_req.retcode && tx_req.recvsize==sizeof(tx)){++sent_frames;last_sent=sceKernelGetSystemTimeLow();}
        if(!epoch_reset && attached && (tx_req.retcode || tx_req.recvsize!=sizeof(tx))) { ready=0; usb_error(1,tx_req.retcode,tx_req.recvsize); }
        if(tx.type==LINK_WIFI_PROFILE)native_profile_clear(&tx,sizeof(tx));
    }
    if(rx_busy && rx_done) {
        rx_busy=rx_done=0;
        sceKernelDcacheInvalidateRange(&rx,sizeof(rx));
        if(!rx_req.retcode && link_valid(&rx,rx_req.recvsize)){++received_frames;last_received=sceKernelGetSystemTimeLow();}
        if(!epoch_reset && attached && (rx_req.retcode || !link_valid(&rx,rx_req.recvsize))) usb_error(2,rx_req.retcode,rx_req.recvsize);
        else if(attached && !epoch_reset && rx.type==LINK_HELLO &&
            rx.length==strlen(native_mode?LINK_NATIVE_HELLO:LINK_BROWSER_HELLO) &&
            !memcmp(rx.payload,native_mode?LINK_NATIVE_HELLO:LINK_BROWSER_HELLO,rx.length)) ready=1;
        else if(attached && !epoch_reset && ready && native_mode && rx.type==LINK_WIFI_SCAN_RESULT)
            native_scan_consume(&scan,&rx);
        else if(attached && !epoch_reset && ready && native_mode && rx.type==LINK_WIFI_PROFILE_RESULT &&
            profile_request && rx.reply_to==profile_request) {
            if(rx.length==8 && rx.payload[0]==1 && !rx.payload[1] && !rx.payload[2] && !rx.payload[3])
                profile_result=link_get_u32(rx.payload+4)?-1:1;
            else profile_result=-1;
            profile_request=0;
        }
        else if(attached && !epoch_reset && ready && rx.type==LINK_WIFI_INFO && rx.length==48) {
            memcpy(wifi_info,rx.payload,48);wifi_info_valid=1;
        }
        else if(attached && !epoch_reset && ready && rx.type==LINK_NET_STATUS && rx.length==8) {
            uplink=link_get_u32(rx.payload)&3;
            memcpy(remote_ip,rx.payload+4,4);
        }
        else if(attached && !epoch_reset && ready && rx.type==LINK_PACKET) {
            int n=packet_link_receive(&packet_rx,&rx);
            if(n>0 && (unsigned)n<=capacity) { memcpy(data,packet_rx.data,n); result=n; }
            else if(n<0) usb_error(3,n,rx.length);
        }
    }
    if(epoch_reset) {
        if(tx_busy || rx_busy) return 0;
        epoch_reset=0;
        return 0;
    }
    if(attached && !tx_busy && (hello_pending || scan_pending || profile_count || packet_tx.total)) {
        if(hello_pending) { link_init(&tx,LINK_HELLO,++seq,0,native_mode?LINK_NATIVE_HELLO:LINK_BROWSER_HELLO); hello_pending=0; }
        else if(scan_pending){tx=scan_request;scan_pending=0;}
        else if(profile_count) {
            native_profile_encode(&tx,++seq,profile_request,&profile,profile_index++);
            if(profile_index==profile_count){profile_count=0;native_profile_clear(&profile,sizeof(profile));}
        }
        else packet_link_next(&packet_tx,&tx,++seq);
        sceKernelDcacheWritebackRange(&tx,sizeof(tx));
        memset(&tx_req,0,sizeof(tx_req)); tx_req.endp=&endpoints[1];
        tx_req.data=&tx; tx_req.size=sizeof(tx); tx_req.func=tx_complete;
        tx_done=0; tx_busy=1;
        int r=sceUsbbdReqSend(&tx_req);
        if(r<0) { tx_busy=0; ready=0; usb_error(4,r,0); }
    }
    if(attached && !rx_busy) {
        sceKernelDcacheInvalidateRange(&rx,sizeof(rx));
        memset(&rx_req,0,sizeof(rx_req)); rx_req.endp=&endpoints[2];
        rx_req.data=&rx; rx_req.size=sizeof(rx); rx_req.func=rx_complete;
        rx_done=0; rx_busy=1;
        int r=sceUsbbdReqRecv(&rx_req);
        if(r<0) { rx_busy=0; usb_error(5,r,0); }
    }
    return result;
}
int vnet_usb_stop(void)
{
    if(active) { if(sceUsbDeactivate(LINK_PID)<0) return -1; active=0; }
    attached=ready=0;
    native_profile_clear(&profile,sizeof(profile));profile_count=profile_request=0;
    if(tx_busy) sceUsbbdReqCancelAll(&endpoints[1]);
    if(rx_busy) sceUsbbdReqCancelAll(&endpoints[2]);
    unsigned start=sceKernelGetSystemTimeLow();
    while(tx_busy || rx_busy) {
        if(tx_done) tx_busy=tx_done=0;
        if(rx_done) rx_busy=rx_done=0;
        if(sceKernelGetSystemTimeLow()-start>2000000u) return -1;
        sceKernelDelayThread(1000);
    }
#define STOP(expr,flag) do { if(flag) { if((expr)<0) return -1; flag=0; } } while(0)
    STOP(sceUsbStop(DRIVER,0,NULL),own);
    STOP(sceUsbStop("USBAccBaseDriver",0,NULL),acc);
    STOP(sceUsbStop(PSP_USBBUS_DRIVERNAME,0,NULL),bus);
    STOP(sceUsbAccUnregisterType(ACC_TYPES),types);
    STOP(sceUsbbdUnregister(&driver),registered);
    return 0;
#undef STOP
}
