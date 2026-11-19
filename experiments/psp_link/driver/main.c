#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspusb.h>
#include <pspusbbus.h>
#include <pspsdk.h>
#include <pspsysclib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "link_protocol.h"
#include "page_file.h"

PSP_MODULE_INFO("PSP ESP Link", PSP_MODULE_KERNEL, 1, 0);

extern int sceUsbAccRegisterType(u16 type);
extern int sceUsbAccUnregisterType(u16 type);

#define DRIVER "PSPWifiLink"
#define ACC_TYPES 3
#define LOG_PATH "ms0:/psp-link.log"

static struct UsbEndpoint endpoints[3] = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
static struct UsbInterface interface = {-1, 0, 1};
static struct UsbData descriptors[2] __attribute__((aligned(64)));
static struct UsbDriver driver;
static link_frame_t tx __attribute__((aligned(64)));
static link_frame_t rx __attribute__((aligned(64)));
static struct UsbdDeviceReq tx_req, rx_req;
static link_frame_t queue[8];
static unsigned queue_head, queue_count;
static volatile int running, attached, speed;
static volatile int tx_busy, rx_busy, tx_done, rx_done;
static SceUID worker = -1;
static u32 start_time;
static unsigned tx_count, rx_count, invalid_count, seq;
static link_file_receiver_t file_receiver;
static unsigned last_file_progress;

struct print_buffer { char *data; unsigned used, capacity; };

static void append_char(void *arg, int ch)
{
    struct print_buffer *p = arg;
    if (ch >= 0 && ch < 256 && p->used + 1 < p->capacity)
        p->data[p->used++] = ch;
}

static void log_line(const char *format, ...)
{
    char message[192], line[224];
    struct print_buffer buffer = {message, 0, sizeof(message)};
    va_list args;
    va_start(args, format);
    prnt(append_char, &buffer, format, args);
    va_end(args);
    message[buffer.used] = 0;
    snprintf(line, sizeof(line), "%8u ms %s\n",
             (unsigned)((sceKernelGetSystemTimeLow() - start_time) / 1000), message);
    pspDebugScreenKprintf("%s", line);
    SceUID fd = sceIoOpen(LOG_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, line, strlen(line));
        sceIoClose(fd);
    }
}

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
    return 0;
}

static int on_detach(int a, int b, int c)
{
    (void)a; (void)b; (void)c;
    attached = 0;
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
    return 0;
}

static struct UsbDriver driver = {
    .name = DRIVER, .endpoints = 3, .endp = endpoints, .intp = &interface,
    .recvctl = on_control, .attach = on_attach, .detach = on_detach,
    .start_func = on_start, .stop_func = on_stop,
};

static int send_frame(unsigned type, unsigned reply_to, const char *text)
{
    if (!attached || queue_count == 8) {
        log_line("TX queue unavailable; type=%u", type);
        return -1;
    }
    link_init(&queue[(queue_head + queue_count) % 8], type, ++seq, reply_to, text);
    queue_count++;
    return 0;
}

static void queue_reply(link_frame_t *frame)
{
    if (queue_count == 8) {
        log_line("File ACK queue full; sender may retry");
        return;
    }
    frame->seq = ++seq;
    queue[(queue_head + queue_count) % 8] = *frame;
    queue_count++;
}

static void pump_tx(void)
{
    int r;
    if (!attached || tx_busy || !queue_count) return;
    tx = queue[queue_head];
    queue_head = (queue_head + 1) % 8;
    queue_count--;
    sceKernelDcacheWritebackRange(&tx, sizeof(tx));
    memset(&tx_req, 0, sizeof(tx_req));
    tx_req.endp = &endpoints[1];
    tx_req.data = &tx;
    tx_req.size = sizeof(tx);
    tx_req.func = tx_complete;
    tx_done = 0;
    tx_busy = 1;
    r = sceUsbbdReqSend(&tx_req);
    if (r < 0) {
        tx_busy = 0;
        log_line("TX submit error %08x", r);
    }
}

static void receive_frame(void)
{
    int r;
    if (!attached || rx_busy) return;
    sceKernelDcacheInvalidateRange(&rx, sizeof(rx));
    memset(&rx_req, 0, sizeof(rx_req));
    rx_req.endp = &endpoints[2];
    rx_req.data = &rx;
    rx_req.size = sizeof(rx);
    rx_req.func = rx_complete;
    rx_done = 0;
    rx_busy = 1;
    r = sceUsbbdReqRecv(&rx_req);
    if (r < 0) {
        rx_busy = 0;
        log_line("RX submit error %08x", r);
    }
}

static void process_completions(void)
{
    if (tx_busy && tx_done) {
        tx_busy = 0;
        tx_done = 0;
        if (tx_req.retcode == 0 && tx_req.recvsize == sizeof(tx)) tx_count++;
        else log_line("TX completion code=%08x bytes=%d", tx_req.retcode, tx_req.recvsize);
    }
    if (rx_busy && rx_done) {
        rx_busy = 0;
        rx_done = 0;
        sceKernelDcacheInvalidateRange(&rx, sizeof(rx));
        if (rx_req.retcode != 0) {
            log_line("RX completion code=%08x bytes=%d", rx_req.retcode, rx_req.recvsize);
        } else if (!link_valid(&rx, rx_req.recvsize)) {
            invalid_count++;
            log_line("Invalid RX frame (%d bytes)", rx_req.recvsize);
        } else {
            rx_count++;
            if (rx.type <= LINK_HELLO)
                log_line("RX type=%u seq=%u reply=%u text=%.*s", rx.type,
                         (unsigned)rx.seq, (unsigned)rx.reply_to, rx.length, rx.payload);
            if (rx.type == LINK_PING) {
                char text[LINK_PAYLOAD_SIZE + 1];
                memcpy(text, rx.payload, rx.length);
                text[rx.length] = 0;
                send_frame(LINK_PONG, rx.seq, text);
            } else if (rx.type == LINK_FILE_BEGIN || rx.type == LINK_FILE_DATA ||
                       rx.type == LINK_FILE_END || rx.type == LINK_FILE_ABORT) {
                if (!attached) return;
                link_frame_t reply;
                link_file_receive(&file_receiver, &page_file_ops, NULL, &rx, &reply);
                if (reply.type == LINK_FILE_ERROR) {
                    log_line("FILE error=%u offset=%u", (unsigned)link_get_u32(reply.payload + 4),
                             (unsigned)file_receiver.offset);
                } else if (rx.type == LINK_FILE_BEGIN) {
                    last_file_progress = 0;
                    log_line("FILE begin size=%u CRC32=%08x -> " PAGE_PATH,
                             (unsigned)file_receiver.size, (unsigned)file_receiver.expected_crc);
                } else if (rx.type == LINK_FILE_END) {
                    log_line("FILE SAVED " PAGE_PATH " bytes=%u CRC32=%08x",
                             (unsigned)file_receiver.offset, (unsigned)file_receiver.expected_crc);
                } else if (file_receiver.offset - last_file_progress >= 4096) {
                    last_file_progress = file_receiver.offset;
                    log_line("FILE progress %u/%u", (unsigned)file_receiver.offset,
                             (unsigned)file_receiver.size);
                }
                queue_reply(&reply);
            }
        }
    }
}

static void drain_transfers(void)
{
    link_file_abort(&file_receiver, &page_file_ops, NULL);
    queue_count = queue_head = 0;
    if (tx_busy) sceUsbbdReqCancelAll(&endpoints[1]);
    if (rx_busy) sceUsbbdReqCancelAll(&endpoints[2]);
    /* Static buffers must remain alive until cancellation callbacks arrive. */
    while (tx_busy || rx_busy) {
        process_completions();
        sceKernelDelayThread(1000);
    }
}

static int run(SceSize size, void *args)
{
    int registered = 0, types = 0, bus = 0, acc = 0, own = 0, active = 0;
    int failed = 0;
    int r, prev_state = -1, prev_attached = -1;
    unsigned prev_buttons = 0;
    u32 next_ping = sceKernelGetSystemTimeLow() + 2000000;
    (void)size; (void)args;
    start_time = sceKernelGetSystemTimeLow();
    pspDebugScreenInit();
    log_line("PSP ESP Link v5 - HTML download receiver");
    log_line("CROSS: ping; TRIANGLE: save page; SQUARE: re-arm; CIRCLE/HOME: exit");
    log_line("Log: " LOG_PATH);
    log_line("Preflight USB=%08x bus=%08x accessory=%08x", sceUsbGetState(),
             sceUsbGetDrvState(PSP_USBBUS_DRIVERNAME), sceUsbGetDrvState("USBAccBaseDriver"));
#define START_STEP(label, expression, flag) do { \
    r = (expression); log_line(label ": %08x", r); \
    if (r < 0) { failed = 1; goto cleanup; } \
    flag = 1; \
} while (0)
    START_STEP("Register link driver", sceUsbbdRegister(&driver), registered);
    START_STEP("Register accessory types 3", sceUsbAccRegisterType(ACC_TYPES), types);
    START_STEP("Start bus", sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, NULL), bus);
    START_STEP("Start accessory", sceUsbStart("USBAccBaseDriver", 0, NULL), acc);
    START_STEP("Start link", sceUsbStart(DRIVER, 0, NULL), own);
    START_STEP("Activate 0282", sceUsbActivate(LINK_PID), active);
    while (running) {
        SceCtrlData pad;
        u32 now = sceKernelGetSystemTimeLow();
        int state = sceUsbGetState();
        process_completions();
        if (state != prev_state) {
            log_line("USB %08x [%c%c%c]", state,
                state & 0x200 ? 'A' : '-', state & 0x20 ? 'C' : '-', state & 2 ? 'E' : '-');
            prev_state = state;
        }
        if (attached != prev_attached) {
            log_line("Attached=%d speed=%d TX=%u RX=%u bad=%u", attached, speed,
                     tx_count, rx_count, invalid_count);
            prev_attached = attached;
            if (!attached) drain_transfers();
            else send_frame(LINK_HELLO, 0, LINK_HTML_HELLO);
        }
        receive_frame();
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~prev_buttons;
        prev_buttons = pad.Buttons;
        if (pressed & PSP_CTRL_TRIANGLE) {
            send_frame(LINK_GET_PAGE, 0, "pspdev.html");
            log_line("Requested page from ESP; output: " PAGE_PATH);
        }
        if (pressed & PSP_CTRL_SQUARE) {
            r = sceUsbDeactivate(LINK_PID);
            log_line("Re-arm deactivate: %08x", r);
            attached = 0;
            drain_transfers();
            prev_attached = -1;
            sceKernelDelayThread(300000);
            r = sceUsbActivate(LINK_PID);
            log_line("Re-arm activate: %08x", r);
            if (r < 0) break;
        }
        if (attached && ((pressed & PSP_CTRL_CROSS) ||
            (!file_receiver.active && !tx_busy && !queue_count && (int32_t)(now - next_ping) >= 0))) {
            send_frame(LINK_PING, 0, "PSP heartbeat");
            next_ping = now + 2000000;
        }
        pump_tx();
        sceKernelDelayThread(10000);
    }
cleanup:
    running = 0;
    log_line("Stopping TX=%u RX=%u bad=%u", tx_count, rx_count, invalid_count);
    if (active) sceUsbDeactivate(LINK_PID);
    attached = 0;
    drain_transfers();
    if (own) sceUsbStop(DRIVER, 0, NULL);
    if (acc) sceUsbStop("USBAccBaseDriver", 0, NULL);
    if (bus) sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, NULL);
    if (types) sceUsbAccUnregisterType(ACC_TYPES);
    if (registered) sceUsbbdUnregister(&driver);
    if (failed) {
        log_line("Startup failed. Save " LOG_PATH "; CIRCLE/HOME exits.");
    }
    log_line("Kernel worker stopped; user launcher owns exit.");
    return 0;
}

int module_start(SceSize size, void *args)
{
    (void)size; (void)args;
    running = 1;
    worker = sceKernelCreateThread("PSPWifiLink", run, 0x20, 0x10000, 0, NULL);
    if (worker < 0) return worker;
    int r = sceKernelStartThread(worker, 0, NULL);
    if (r < 0) sceKernelDeleteThread(worker);
    return r;
}

int module_stop(SceSize size, void *args)
{
    (void)size; (void)args;
    running = 0;
    if (worker >= 0) {
        if (worker == sceKernelGetThreadId()) return -1;
        int r = sceKernelWaitThreadEnd(worker, NULL);
        if (r < 0) return r;
        r = sceKernelDeleteThread(worker);
        if (r < 0) return r;
        worker = -1;
    }
    return 0;
}
