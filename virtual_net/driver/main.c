#include "../../common/esp_diagnostics.h"
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsdk.h>
#include <pspsysmem_kernel.h>
#include <pspsysclib.h>
#include <pspdebug.h>
#include <systemctrl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../control.h"
#include "interface.h"
#include "mbuf.h"
#include "log_queue.h"
#include "packet_trace.h"
#include "frame_queue.h"
#ifdef VNET_USB_LAB
#include "usb_transport.h"
#include "ssl_trace.h"
#include <psprtc.h>
static ssl_trace_t ssl_capture;
static int ssl_capture_enabled;
#endif
#include "lab_gateway.h"
#include "ifhandle_661_guard.h"

PSP_MODULE_INFO("PSP Virtual Interface", PSP_MODULE_KERNEL, 0, 12);

#ifdef VNET_USB_LAB
static volatile vnet_start_result_t *usb_status;
#endif
static vnet_session_t session __attribute__((aligned(64)));
static vnet_ops_t ops;
static void *(*native_dequeue)(vnet_record_t *);
static void (*native_enqueue)(vnet_record_t *, void *);
static void *(*native_mget)(int, int);
static void (*native_mfreem)(void *);
static SceUID worker = -1;
static volatile int running, lab_live, packet_fault;
static volatile unsigned tx_packets, rx_packets, rejected_packets;
static void reject_packet(void)
{int irq=sceKernelCpuSuspendIntr();++rejected_packets;sceKernelCpuResumeIntr(irq);}
static unsigned char tx_frame[VNET_ETH_MAX], rx_frame[VNET_ETH_MAX];
static int (*native_complete)(vnet_record_t *, unsigned, int);
static int (*native_ioctl)(uint32_t, unsigned, void *);
static volatile unsigned up_calls, down_calls, send_calls, ioctl_calls, bad_record;
static volatile int last_complete_result;
static volatile unsigned last_ioctl_command;

static SceUID log_worker=-1;
static volatile int log_running;
static vnet_log_queue_t log_queue;
static void write_log(const char *text)
{
    SceUID fd=sceIoOpen(!strncmp(text,"SSL_",4)?"ms0:/ssl-capture.log":"ms0:/psp-vnet.log",PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0600);
    if(fd>=0){sceIoWrite(fd,text,strlen(text));sceIoClose(fd);}
}
static int log_task(SceSize size,void *args)
{
    (void)size;(void)args;char text[VNET_LOG_WIDTH];unsigned synced=sceKernelGetSystemTimeLow();
    for(;;) {
        int irq=sceKernelCpuSuspendIntr();int found=vnet_log_pop(&log_queue,text);sceKernelCpuResumeIntr(irq);
        if(found) {
            write_log(text);unsigned now=sceKernelGetSystemTimeLow();
            if(now-synced>=1000000u){sceIoSync("ms0:",0);synced=now;}
        }
        else if(!log_running)break;
        else sceKernelDelayThread(20000);
    }
    sceIoSync("ms0:",0);return 0;
}
struct print_buffer { char *text; unsigned used, capacity; };
static void append_char(void *arg, int ch)
{
    struct print_buffer *b = arg;
    if (ch >= 0 && ch < 256 && b->used + 1 < b->capacity)
        b->text[b->used++] = ch;
}

static void record(const char *format, ...)
{
    if(!ESP1000_DIAGNOSTICS)return;
    char text[224];
    struct print_buffer b = {text, 0, sizeof(text) - 1};
    va_list args;
    va_start(args, format);
    prnt(append_char, &b, format, args);
    va_end(args);
    text[b.used++] = '\n';
    text[b.used] = 0;
    if(log_worker>=0) {
        int irq=sceKernelCpuSuspendIntr();vnet_log_push(&log_queue,text);sceKernelCpuResumeIntr(irq);
    } else {write_log(text);sceIoSync("ms0:",0);}
}

#ifdef VNET_USB_LAB
static void trace_ssl_event(void *ctx,const ssl_trace_event_t *e)
{
    (void)ctx;
    record("SSL_TRACE time_us=%u flow=%u peer=%u.%u.%u.%u local=%u remote=%u dir=%s event=%s a=%08x b=%08x c=%08x",
        sceKernelGetSystemTimeLow(),e->id,e->peer>>24,(e->peer>>16)&255,(e->peer>>8)&255,e->peer&255,
        e->local_port,e->remote_port,e->direction?"RX":"TX",ssl_trace_name(e->event),e->a,e->b,e->c);
}
static void trace_packet(const char *direction,unsigned count,const unsigned char *frame,unsigned length)
{
    if(ssl_capture_enabled)ssl_trace_feed(&ssl_capture,!strcmp(direction,"TX"),frame,length,trace_ssl_event,NULL);
    if(count>64)return;
    packet_trace_t p;if(!packet_trace_parse(&p,frame,length))return;
    record("TCP_TRACE %s #%u %u.%u.%u.%u:%u -> %u.%u.%u.%u:%u proto=%u flags=%02x seq=%08x ack=%08x data=%u crc=%u/%u",
        direction,count,p.source[0],p.source[1],p.source[2],p.source[3],p.sport,
        p.destination[0],p.destination[1],p.destination[2],p.destination[3],p.dport,
        p.protocol,p.flags,p.seq,p.ack,p.payload,p.ip_ok,p.tcp_ok);
}

#endif

/* These callbacks can run on another native thread. Do not print, allocate,
 * inspect request payloads or invoke USB here. This registration-only driver
 * only brings up the local lab fixture; it never reports ESP/Wi-Fi state. */
static void count_call(vnet_record_t *record, volatile unsigned *counter)
{
    int flags = sceKernelCpuSuspendIntr();
    ++*counter;
    if (record != &session.record) ++bad_record;
    sceKernelCpuResumeIntr(flags);
}
static int interface_up(vnet_record_t *r)
{
    count_call(r, &up_calls);
    if (r != &session.record) return -1;
    int ok = running && !packet_fault;
#ifdef VNET_USB_LAB
    ok = ok && vnet_usb_ready();
#endif
    lab_live = ok;
    last_complete_result = native_complete(r, ok ? 1 : 4, ok ? 0 : -1);
    return ok ? 0 : -1;
}
static int interface_down(vnet_record_t *r)
{
    count_call(r, &down_calls);
    lab_live = 0;
    if (r == &session.record) last_complete_result = native_complete(r, 2, 0);
    return 0;
}
static int interface_send(vnet_record_t *r)
{ count_call(r, &send_calls); return r == &session.record ? 0 : -1; }
static int interface_ioctl(vnet_record_t *r, unsigned command, void *request, void *nested)
{
    last_ioctl_command = command;
    (void)request; (void)nested;
    count_call(r, &ioctl_calls);
    int result = vnet_complete_address_ioctl(&session.record, r, command, native_complete);
    if (r == &session.record && (command == 2 || command == 3))
        last_complete_result = result;
    return result;
}

static unsigned resolve(const SceModule *mod, unsigned nid, unsigned offset)
{
    unsigned address = sctrlHENFindFunction("sceNet_Service", "sceNetIfhandle_driver", nid);
    return address == mod->text_addr + offset ? address : 0;
}

static int resolve_abi(void)
{
    if ((sceKernelDevkitVersion() != 0x06060110 && sceKernelDevkitVersion() != 0x06060010) || sceKernelGetModel() != 10) {
        record("REFUSED: requires PSP-E1000 and 6.61");
        return -1;
    }
    SceModule *mod = sceKernelFindModuleByName("sceNet_Service");
    SceModule *net = sceKernelFindModuleByName("sceNet_Library");
    if (!mod || !net || mod->text_size != 0x3cf0 || mod->version[0] != 7
        || mod->version[1] != 1 || net->text_size != 0xb1a4) {
        record("REFUSED: expected COMMON/NET modules are unavailable or differ");
        return -1;
    }
    for (unsigned i = 0; i < sizeof(ifhandle_anchors) / sizeof(ifhandle_anchors[0]); ++i) {
        unsigned actual = *(volatile unsigned *)(mod->text_addr + ifhandle_anchors[i].offset);
        if (actual != ifhandle_anchors[i].expected) {
            record("REFUSED: ifhandle ABI anchor %04x differs", ifhandle_anchors[i].offset);
            return -1;
        }
    }
    ops.create = (void *)resolve(mod, IF_CREATE_NID, IF_CREATE_OFFSET);
    ops.attach = (void *)resolve(mod, IF_ATTACH_NID, IF_ATTACH_OFFSET);
    ops.detach = (void *)resolve(mod, IF_DETACH_NID, IF_DETACH_OFFSET);
    ops.destroy = (void *)resolve(mod, IF_DESTROY_NID, IF_DESTROY_OFFSET);
    native_ioctl = (void *)resolve(mod, IF_IOCTL_NID, IF_IOCTL_OFFSET);
    native_complete = (void *)resolve(mod, IF_COMPLETE_NID, IF_COMPLETE_OFFSET);
    native_dequeue = (void *)resolve(mod, IF_DEQUEUE_NID, IF_DEQUEUE_OFFSET);
    native_enqueue = (void *)resolve(mod, IF_ENQUEUE_NID, IF_ENQUEUE_OFFSET);
    native_mget = (void *)resolve(mod, IF_MGET_NID, IF_MGET_OFFSET);
    native_mfreem = (void *)resolve(mod, IF_MFREEM_NID, IF_MFREEM_OFFSET);
    if (!native_dequeue || !native_enqueue || !native_mget || !native_mfreem
        || !native_complete || !ops.create || !ops.attach || !ops.detach || !ops.destroy || !native_ioctl) {
        record("REFUSED: ifhandle library/NID addresses differ");
        return -1;
    }
    record("ABI checks passed; ifhandle=%08x NET=%08x", mod->text_addr, net->text_addr);
    return 0;
}

static int create_record(vnet_record_t *r)
{
    record("CREATE_BEGIN");
    int result = ops.create(r);
    record("CREATE_END result=%08x internal=%08x", result, r->internal);
    if (result == 0 && r->internal)
        record("IF_SEMAPHORES completion=%08x lock=%08x",
               *(unsigned *)(r->internal + 0x100), *(unsigned *)(r->internal + 0x104));
    return result;
}
static int attach_record(vnet_record_t *r, const unsigned char *mac, const char *name)
{
    record("ATTACH_BEGIN name=%s", name);
    int result = ops.attach(r, mac, name);
    record("ATTACH_END result=%08x", result);
    return result;
}
static int detach_record(vnet_record_t *r)
{
    record("DETACH_BEGIN");
    int result = ops.detach(r);
    record("DETACH_END result=%08x", result);
    return result;
}
static int destroy_record(vnet_record_t *r)
{
    record("DESTROY_BEGIN");
    int result = ops.destroy(r);
    record("DESTROY_END result=%08x", result);
    return result;
}
static const vnet_ops_t traced_ops = {create_record, attach_record, detach_record, destroy_record};

/* Native mbufs live in sceNetInit's positive-address user pool. Their metadata
 * comes from Sony's TX queue/allocator, never from USB-supplied pointers. */
static int pool_range(unsigned address, size_t length)
{
    return address >= 0x08800000 && length <= 0x04000000
        && address <= 0x0c000000 - length;
}
static int pool_read(void *ctx, uint32_t address, void *out, size_t length)
{
    (void)ctx;
    if (!pool_range(address, length)) return 0;
    memcpy(out, (void *)address, length);
    return 1;
}
static int pool_write(void *ctx, uint32_t address, const void *data, size_t length)
{
    (void)ctx;
    if (!pool_range(address, length)) return 0;
    memcpy((void *)address, data, length);
    return 1;
}
static uint32_t pool_get(void *ctx)
{ (void)ctx; return (unsigned)native_mget(0, 1); }
static void pool_free(void *ctx, uint32_t head)
{ (void)ctx; native_mfreem((void *)head); }
static const vnet_mbuf_ops_t mbufs = {NULL, pool_read, pool_write, pool_get, pool_free};

#ifdef VNET_USB_LAB
static SceUID native_rx_worker=-1,native_tx_worker=-1;
static volatile int native_rx_running,native_tx_running;
static vnet_frame_queue_t receive_queue,transmit_queue;
static volatile unsigned rx_stage,tx_stage,rx_injected;
static unsigned queue_pop(vnet_frame_queue_t *queue,void *frame)
{int irq=sceKernelCpuSuspendIntr();unsigned n=vnet_frame_pop(queue,frame);sceKernelCpuResumeIntr(irq);return n;}
static int queue_push(vnet_frame_queue_t *queue,const void *frame,unsigned n)
{int irq=sceKernelCpuSuspendIntr();int ok=vnet_frame_push(queue,frame,n);sceKernelCpuResumeIntr(irq);return ok;}
static int receive_native(SceSize size,void *args)
{
    (void)size;(void)args;unsigned char frame[VNET_ETH_MAX];
    while(native_rx_running) {
        rx_stage=0;unsigned n=queue_pop(&receive_queue,frame);
        if(!n){sceKernelDelayThread(1000);continue;}
        if(!lab_live){reject_packet();continue;}
        rx_stage=1;unsigned mbuf=vnet_mbuf_make(&mbufs,frame,n,session.record.internal);
        if(!mbuf){reject_packet();continue;}
        rx_stage=2;native_enqueue(&session.record,(void *)mbuf);++rx_injected;
    }
    rx_stage=0;return 0;
}
static int transmit_native(SceSize size,void *args)
{
    (void)size;(void)args;unsigned char frame[VNET_ETH_MAX];
    while(native_tx_running) {
        tx_stage=0;
        int irq=sceKernelCpuSuspendIntr();int space=vnet_frame_space(&transmit_queue);sceKernelCpuResumeIntr(irq);
        if(!lab_live || !vnet_usb_ready() || !space){sceKernelDelayThread(1000);continue;}
        tx_stage=1;unsigned head=(unsigned)vnet_dequeue_ready(&session,native_dequeue);
        if(!head){sceKernelDelayThread(1000);continue;}
        size_t length=0;tx_stage=2;
        if(!vnet_mbuf_copy(&mbufs,head,frame,sizeof(frame),&length)) {
            reject_packet();packet_fault=1;lab_live=0;
            record("PACKET_FAULT: invalid native TX chain retained until NET teardown");break;
        }
        tx_stage=3;native_mfreem((void *)head);
        if(!queue_push(&transmit_queue,frame,length))reject_packet();
    }
    tx_stage=0;return 0;
}
static int packet_worker(SceSize size,void *args)
{
    (void)size;(void)args;int previous_ready=-1;unsigned previous_errors=0;
    unsigned seen_up=~0u,seen_down=~0u,seen_ioctl=~0u,last_stats=0;
    while(running) {
        /* This owner performs USB/copy/queue operations only. Native NET calls
         * cannot delay bulk endpoint re-arming or outgoing ACK transmission. */
        int n=vnet_usb_poll(rx_frame,sizeof(rx_frame));
        if(n>0) {
            ++rx_packets;trace_packet("RX",rx_packets,rx_frame,n);
            if(!lab_live || !queue_push(&receive_queue,rx_frame,n))reject_packet();
        }
        unsigned errors=vnet_usb_errors();
        if(errors!=previous_errors) {
            unsigned stage,bytes;int code;vnet_usb_last_error(&stage,&code,&bytes);
            record("USB_PACKET_ERRORS=%u stage=%u code=%08x bytes=%u",errors,stage,code,bytes);previous_errors=errors;
            if(stage==2){vnet_usb_progress_t snapshot;vnet_usb_progress(&snapshot);record("USB_FRAME_CHECKS=%02x",snapshot.rx_checks);}
        }
        int ready=vnet_usb_ready();
        int irq=sceKernelCpuSuspendIntr();++usb_status->revision;
        unsigned char ip[4],radio[48];unsigned uplink=vnet_usb_uplink(ip);int radio_valid=vnet_usb_wifi_info(radio);
        memcpy((void *)usb_status->remote_ip,ip,4);memcpy((void *)usb_status->wifi_info,radio,48);
        usb_status->flags&=~(VNET_USB_READY|VNET_UPLINK_READY|VNET_REMOTE_READY|VNET_WIFI_INFO_READY);
        if(ready)usb_status->flags|=VNET_USB_READY;
        if(ready && radio_valid)usb_status->flags|=VNET_WIFI_INFO_READY;
        if(ready && (uplink&1))usb_status->flags|=VNET_UPLINK_READY;
        if(ready && (uplink&2))usb_status->flags|=VNET_REMOTE_READY;
        ++usb_status->revision;sceKernelCpuResumeIntr(irq);
        if(ready!=previous_ready) {
            record("USB_PACKET_READY=%d",ready);previous_ready=ready;
            if(!ready){irq=sceKernelCpuSuspendIntr();receive_queue.read=receive_queue.write;transmit_queue.read=transmit_queue.write;sceKernelCpuResumeIntr(irq);}
        }
        if(seen_up!=up_calls || seen_down!=down_calls || seen_ioctl!=ioctl_calls) {
            seen_up=up_calls;seen_down=down_calls;seen_ioctl=ioctl_calls;
            record("CALLBACK_STATE up=%u down=%u ioctl=%u command=%08x complete=%08x",seen_up,seen_down,seen_ioctl,last_ioctl_command,last_complete_result);
        }
        if(vnet_usb_can_send()) {
            n=queue_pop(&transmit_queue,tx_frame);
            if(n){++tx_packets;trace_packet("TX",tx_packets,tx_frame,n);if(!vnet_usb_send(tx_frame,n))reject_packet();}
        }
        unsigned now=sceKernelGetSystemTimeLow();
        if(now-last_stats>=2000000u) {
            irq=sceKernelCpuSuspendIntr();unsigned queued_rx=receive_queue.write-receive_queue.read,queued_tx=transmit_queue.write-transmit_queue.read;sceKernelCpuResumeIntr(irq);
            record("PIPE rx_queue=%u tx_queue=%u rx_stage=%u tx_stage=%u injected=%u rejected=%u",queued_rx,queued_tx,rx_stage,tx_stage,rx_injected,rejected_packets);last_stats=now;
            vnet_usb_progress_t usb;vnet_usb_progress(&usb);
            record("USB_PIPE busy=%u/%u packet=%u/%u completed=%u/%u age_us=%u/%u",usb.tx_busy,usb.rx_busy,usb.packet_offset,usb.packet_total,usb.tx_frames,usb.rx_frames,usb.tx_age_us,usb.rx_age_us);
            record("USB_DIAG attached=%u epoch=%u attach/detach=%u/%u pending_us=%u/%u result=%08x/%08x bytes=%u/%u checks=%02x",
                   usb.attached,usb.epoch_reset,usb.attaches,usb.detaches,usb.tx_pending_us,usb.rx_pending_us,
                   usb.tx_result,usb.rx_result,usb.tx_bytes,usb.rx_bytes,usb.rx_checks);
            record("USB_TX_HEADER checked=%u before=%02x after=%02x changed=%04x uncached=%08x/%08x bad=%u/%u/%u",
                   usb.tx_checked,usb.tx_pre_checks,usb.tx_post_checks,usb.tx_changed,usb.tx_uncached_pre,usb.tx_uncached_post,usb.tx_bad_before,usb.tx_bad_after,usb.tx_bad_uncached);
        }
        sceKernelDelayThread(1000);
    }
    return 0;
}
static int stop_native(SceUID *id,volatile int *active)
{
    *active=0;if(*id<0)return 0;
    unsigned timeout=2000000;
    if(sceKernelWaitThreadEnd(*id,&timeout)<0)return -1;
    if(sceKernelDeleteThread(*id)<0)return -1;
    *id=-1;return 0;
}
#else
static int packet_worker(SceSize size, void *args)
{
    (void)size; (void)args;
    unsigned seen_up = 0, seen_down = 0, seen_ioctl = ioctl_calls;
    while (running) {
        if (seen_up != up_calls || seen_down != down_calls || seen_ioctl != ioctl_calls) {
            seen_up = up_calls; seen_down = down_calls; seen_ioctl = ioctl_calls;
            record("CALLBACK_STATE up=%u down=%u ioctl=%u command=%08x complete=%08x pending=%u",
                   seen_up, seen_down, seen_ioctl, last_ioctl_command, last_complete_result,
                   *(volatile unsigned *)(session.record.internal + 0x108));
        }
        for (unsigned count = 0; count < 8 && running; ++count) {
            /* Native dequeue signals NET even when empty. Never poll it as a
             * queue-empty query: that continuously wakes the native NET thread. */
            unsigned head = (unsigned)vnet_dequeue_ready(&session, native_dequeue);
            if (!head) break;
            size_t length = 0;
            ++tx_packets;
            if (!vnet_mbuf_copy(&mbufs, head, tx_frame, sizeof(tx_frame), &length)) {
                /* A malformed chain cannot safely be walked again by mFreem.
                 * Quarantine it until native NET bulk-pool teardown. */
                reject_packet();
                packet_fault = 1; lab_live = 0; running = 0;
                record("PACKET_FAULT: invalid native TX chain; retained for NET pool teardown");
                break;
            }
            native_mfreem((void *)head);
            size_t reply_length = lab_live ? vnet_lab_reply(tx_frame, length,
                                                rx_frame, sizeof(rx_frame)) : 0;
            if (tx_packets <= 12)
                record("LAB_TX packet=%u bytes=%u type=%02x%02x reply=%u",
                       tx_packets, (unsigned)length, tx_frame[12], tx_frame[13], (unsigned)reply_length);
            if (!reply_length) { reject_packet(); continue; }
            head = vnet_mbuf_make(&mbufs, rx_frame, reply_length, session.record.internal);
            if (!head) { reject_packet(); continue; }
            /* Enqueue consumes the entire packet on every path, including its
             * native drop filters. Never free it again after this handoff. */
            native_enqueue(&session.record, (void *)head);
            ++rx_packets;
        }
        if (running) sceKernelDelayThread(1000);
    }
    return 0;
 }
#endif

int module_start(SceSize size, void *args)
{
    vnet_start_args_t request;
    if (size != sizeof(request) || !args) return -1;
    memcpy(&request, args, sizeof(request));
    if (request.magic != VNET_START_MAGIC || (request.result_address & 3)
        || request.result_address < 0x08800000
        || request.result_address > 0x0c000000 - sizeof(vnet_start_result_t)) return -1;
    volatile vnet_start_result_t *out = (void *)request.result_address;
    memset((void *)out,0,sizeof(*out));
    out->result = -1;
    out->flags = 0;
    memset((void *)out->remote_ip,0,4);
    record("BEGIN virtual interface v0.12 (packet lab; backend selected at build)");
#ifdef VNET_USB_LAB
    int ssl_flag=ESP1000_DIAGNOSTICS?sceIoOpen("ms0:/ssl-capture.enable",PSP_O_RDONLY,0):-1;
    ssl_capture_enabled=ssl_flag>=0;
    if(ssl_flag>=0)sceIoClose(ssl_flag);
    if(ssl_capture_enabled) {
        record("SSL_CAPTURE BEGIN version=1 tcp_port=443 flows=%u prefix_bytes=%u event_limit=%u",SSL_TRACE_FLOWS,SSL_TRACE_PREFIX,SSL_TRACE_EVENTS);
        ScePspDateTime clock;memset(&clock,0,sizeof(clock));int clock_result=sceRtcGetCurrentClock(&clock,0);
        record("SSL_CAPTURE CLOCK result=%08x utc=%04u-%02u-%02uT%02u:%02u:%02u",clock_result,clock.year,clock.month,clock.day,clock.hour,clock.minute,clock.second);
    }
#endif
    if (resolve_abi() < 0) return 0;
    int r = vnet_create(&session, &traced_ops);
    if (r != 0) { out->result = r; return 0; }
    session.record.up = (unsigned)interface_up;
    session.record.down = (unsigned)interface_down;
    session.record.send = (unsigned)interface_send;
    session.record.ioctl = (unsigned)interface_ioctl;
    r = vnet_attach(&session, &traced_ops, vnet_lab_psp_mac, "usb");
    out->result = r;
    if (r != 0) return 0;
    out->flags |= VNET_REGISTERED;
    /* Own-interface unsupported request: verifies native internal-to-external
     * handle dispatch without interpreting private request payloads. */
    unsigned payload[8] = {0};
    record("IOCTL_SELF_CHECK_BEGIN");
    r = native_ioctl(session.record.internal, 0xffffffffu, payload);
    record("IOCTL_SELF_CHECK_END result=%08x calls=%u bad_record=%u", r, ioctl_calls, bad_record);
    if (r == -1 && ioctl_calls == 1 && bad_record == 0) out->flags |= VNET_CALLBACK_OK;
    else out->result = -1;
    if (out->result != 0) return 0;
#ifdef VNET_USB_LAB
    usb_status = out;
    r = vnet_usb_start();
    record("USB_START result=%08x; wait for ESP packet capability before SQUARE", r);
    if (r < 0) { out->result = r; return 0; }
#endif
    if(ESP1000_DIAGNOSTICS) {
    log_worker=sceKernelCreateThread("VirtualNetLog",log_task,0x32,0x4000,0,NULL);
    if(log_worker<0){out->result=log_worker;return 0;}
    log_running=1;r=sceKernelStartThread(log_worker,0,NULL);
    if(r<0){log_running=0;sceKernelDeleteThread(log_worker);log_worker=-1;out->result=r;return 0;}
    }
#ifdef VNET_USB_LAB
    native_rx_worker=sceKernelCreateThread("VirtualNetReceive",receive_native,0x25,0x4000,0,NULL);
    if(native_rx_worker<0){out->result=native_rx_worker;return 0;}
    native_rx_running=1;r=sceKernelStartThread(native_rx_worker,0,NULL);
    if(r<0){native_rx_running=0;sceKernelDeleteThread(native_rx_worker);native_rx_worker=-1;out->result=r;return 0;}
    native_tx_worker=sceKernelCreateThread("VirtualNetTransmit",transmit_native,0x25,0x4000,0,NULL);
    if(native_tx_worker<0){out->result=native_tx_worker;return 0;}
    native_tx_running=1;r=sceKernelStartThread(native_tx_worker,0,NULL);
    if(r<0){native_tx_running=0;sceKernelDeleteThread(native_tx_worker);native_tx_worker=-1;out->result=r;return 0;}
#endif
    worker = sceKernelCreateThread("VirtualPacketLab", packet_worker, 0x24, 0x4000, 0, NULL);
    if (worker < 0) { out->result = worker; return 0; }
    running = 1;
    r = sceKernelStartThread(worker, 0, NULL);
    if (r < 0) {
        running = 0; sceKernelDeleteThread(worker); worker = -1;
        out->result = r; return 0;
    }
    out->flags |= VNET_LAB_BACKEND;
    record("REGISTERED: virtual usb interface; native callback=%s; link remains down",
           (out->flags & VNET_CALLBACK_OK) ? "verified" : "FAILED");
    return 0;
}

int module_stop(SceSize size, void *args)
{
    unsigned magic = 0;
    if (size == sizeof(magic) && args) memcpy(&magic, args, sizeof(magic));
    if (magic == VNET_NETTERM_MAGIC) {
        /* Trusted launcher sends this only after successful sceNetTerm.
         * Its native thread, interface registry and backing pool are gone. */
        if (worker >= 0 || log_worker>=0) return VNET_STOP_UNSAFE;
#ifdef VNET_USB_LAB
        if(native_rx_worker>=0 || native_tx_worker>=0)return VNET_STOP_UNSAFE;
#endif
        vnet_forget_after_netterm(&session);
        record("Released bookkeeping after confirmed native NET termination");
        return 0;
    }
#ifdef VNET_USB_LAB
    /* Keep USB and native TX draining while any RX enqueue finishes. */
    if(stop_native(&native_rx_worker,&native_rx_running)<0 || stop_native(&native_tx_worker,&native_tx_running)<0) {
        record("NATIVE_WORKER_STOP_UNSAFE rx_stage=%u tx_stage=%u",rx_stage,tx_stage);return VNET_STOP_UNSAFE;
    }
#endif
    running = 0;
#ifdef VNET_USB_LAB
    if (usb_status) usb_status->flags &= ~VNET_USB_READY;
#endif
    if (worker >= 0) {
        int r = sceKernelWaitThreadEnd(worker, NULL);
        if (r < 0) { record("Worker join failed: %08x", r); return VNET_STOP_UNSAFE; }
        sceKernelDeleteThread(worker); worker = -1;
    }
#ifdef VNET_USB_LAB
    if(ssl_capture_enabled){ssl_trace_finish(&ssl_capture,trace_ssl_event,NULL);record("SSL_CAPTURE END evictions=%u log_dropped=%u",ssl_capture.evictions,log_queue.dropped);}
    int usb_stop = vnet_usb_stop();
    record("USB_STOP result=%08x", usb_stop);
    if (usb_stop < 0) return VNET_STOP_UNSAFE;
#endif
    record("LAB_STATS tx=%u rx=%u rejected=%u fault=%u",
           tx_packets, rx_packets, rejected_packets, packet_fault);
    record("STOP callbacks up=%u down=%u send=%u ioctl=%u bad_record=%u",
           up_calls, down_calls, send_calls, ioctl_calls, bad_record);
    int r = vnet_close(&session, &traced_ops);
    record("STOP_END result=%08x", r);
    if(log_worker>=0) {
        log_running=0;
        if(sceKernelWaitThreadEnd(log_worker,NULL)<0)return VNET_STOP_UNSAFE;
        sceKernelDeleteThread(log_worker);log_worker=-1;
        record("LOG_DROPPED=%u",log_queue.dropped);
        sceIoSync("ms0:",0);
    }
    return r;
}
