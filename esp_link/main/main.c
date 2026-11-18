#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_private/usb_phy.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "tusb.h"
#include "host/usbh_pvt.h"
#include "host/hcd.h"
#include "link_protocol.h"
#include "link_usb.h"
#include "network_page.h"
#include "native_scan.h"
#include "native_profile.h"
#include "native_wifi_scan.h"
#include "usb_router.h"
#include "packet_link.h"
#include "lab_gateway.h"
#include "host_wakeup.h"

void local_tcp_start(void);

#define VBUS_GPIO GPIO_NUM_4
#define TRANSFER_WATCHDOG_MS 5000
static packet_link_t ethernet_rx, ethernet_tx;
static link_frame_t ethernet_fragment;
static bool ethernet_mode, ethernet_pending, ethernet_network, ethernet_metadata;
static bool router_link_pending;
static int64_t next_net_status;
static bool native_wifi;
static unsigned native_epoch;
static native_profile_receiver_t profile_receiver;
static int64_t profile_received_at;
static uint32_t ethernet_id, ethernet_received, ethernet_replied, ethernet_bad;
static uint8_t ethernet_reply[PACKET_MAX];
static const char *TAG = "psp-link";

typedef struct { char text[96]; } command_t;
static QueueHandle_t commands, outgoing;
/* USB state belongs exclusively to app_main/tuh_task and its callbacks. */
static uint8_t device, candidate, ep_in, ep_out, itf_number;
static bool mounted, rx_busy, tx_busy, automatic = true, vbus_on;
static bool reset_requested;
static uint32_t seq, tx_count, rx_count, bad_count, dropped;
static int64_t tx_started, ping_sent;
static uint32_t ping_seq;
static link_frame_t tx, rx;
static tusb_control_request_t auth_request;
static uint8_t auth_data[64];
static bool auth_busy, auth_pending;
static unsigned auth_attempts;
static int64_t auth_due;
/* Diagnostic counters only: event hook may run in interrupt context. */
static volatile uint32_t attach_events, remove_events, transfer_events;
static uint32_t watchdog_resets;
static int64_t last_rx_completed, last_tx_completed;
static network_page_t page;
static bool html_receiver, file_wanted = true, file_active, file_waiting;
static uint32_t file_session, file_offset, file_expected_offset;
static unsigned file_stage, file_retries;
static int64_t file_deadline;
static link_frame_t file_request;
static void file_acknowledged(const link_frame_t *ack);

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static void set_vbus(bool enabled)
{
    gpio_set_level(VBUS_GPIO, enabled);
    vbus_on = enabled;
    ESP_LOGI(TAG, "GPIO4=%d (unwired in current two-cable setup; not cable VBUS)", enabled);
}

void tuh_event_hook_cb(uint8_t rhport, uint32_t event, bool in_isr)
{
    (void)rhport;
    /* Never log or call blocking routines from this hook. */
    if (event == HCD_EVENT_DEVICE_ATTACH) attach_events++;
    else if (event == HCD_EVENT_DEVICE_REMOVE) remove_events++;
    else if (event == HCD_EVENT_XFER_COMPLETE) transfer_events++;
    host_wakeup_signal(in_isr);
}

bool tuh_enum_descriptor_configuration_cb(uint8_t address, uint8_t index,
                                          const tusb_desc_configuration_t *config)
{
    ESP_LOGI(TAG, "CONFIG address=%u index=%u total=%u interfaces=%u",
             address, index, config->wTotalLength, config->bNumInterfaces);
    return true;
}

static void enqueue(unsigned type, unsigned reply_to, const char *text)
{
    link_frame_t frame;
    link_init(&frame, type, ++seq, reply_to, text);
    if (xQueueSend(outgoing, &frame, 0) != pdTRUE) {
        dropped++;
        ESP_LOGW(TAG, "TX queue full; dropped seq=%" PRIu32, frame.seq);
    }
}

static bool class_init(void) { return true; }
static bool class_deinit(void) { return true; }

void tuh_enum_descriptor_device_cb(uint8_t address, const tusb_desc_device_t *desc)
{
    ESP_LOGI(TAG, "USB address=%u VID:PID=%04x:%04x", address, desc->idVendor, desc->idProduct);
    if (desc->idVendor == LINK_VID && desc->idProduct == LINK_PID) candidate = address;
}

static uint16_t class_open(uint8_t rhport, uint8_t address,
                           const tusb_desc_interface_t *itf, uint16_t size)
{
    uint8_t in, out;
    (void)rhport;
    if (address != candidate || device) return 0;
    const uint8_t *bytes = (const uint8_t *)itf;
    unsigned length = link_usb_endpoints(bytes, size, &in, &out);
    if (!length) return 0;
    for (unsigned pos = 9; pos < length; pos += bytes[pos]) {
        if (bytes[pos + 1] == TUSB_DESC_ENDPOINT &&
            !tuh_edpt_open(address, (const tusb_desc_endpoint_t *)(bytes + pos))) {
            tuh_edpt_close(address, in);
            tuh_edpt_close(address, out);
            return 0;
        }
    }
    device = address;
    ep_in = in;
    ep_out = out;
    itf_number = itf->bInterfaceNumber;
    ESP_LOGI(TAG, "LINK interface=%u bulk IN=%02x OUT=%02x", itf_number, in, out);
    return length;
}

static bool class_configure(uint8_t address, uint8_t number)
{
    usbh_driver_set_config_complete(address, number);
    return true;
}

static void class_close(uint8_t address)
{
    if (address == candidate) candidate = 0;
    if (address != device) return;
    ESP_LOGW(TAG, "PSP detached TX=%" PRIu32 " RX=%" PRIu32, tx_count, rx_count);
    native_wifi=false;++native_epoch;native_profile_clear(&profile_receiver,sizeof(profile_receiver));
    ethernet_mode = ethernet_pending = ethernet_network = ethernet_metadata = false;
    router_link_pending=true;
    ethernet_rx.total = ethernet_tx.total = 0;
    mounted = rx_busy = tx_busy = auth_busy = auth_pending = false;
    html_receiver = file_active = file_waiting = false;
    file_wanted = true;
    device = ep_in = ep_out = 0;
    ping_seq = 0;
    xQueueReset(outgoing);
}

static bool class_transfer(uint8_t address, uint8_t endpoint,
                            xfer_result_t result, uint32_t size)
{
    if (address != device) return false;
    if (endpoint == ep_out) {
        tx_busy = false;
        if (result == XFER_RESULT_SUCCESS && size == sizeof(tx)) {
            tx_count++;
            last_tx_completed = now_ms();
            if (tx.type == LINK_PING) {
                ping_seq = tx.seq;
                ping_sent = now_ms();
            }
            if (tx.type <= LINK_HELLO)
                ESP_LOGI(TAG, "TX type=%u seq=%" PRIu32 " reply=%" PRIu32,
                         tx.type, tx.seq, tx.reply_to);
        } else {
            ESP_LOGE(TAG, "TX result=%d bytes=%" PRIu32, result, size);
            reset_requested = true;
        }
    } else if (endpoint == ep_in) {
        rx_busy = false;
        if (result != XFER_RESULT_SUCCESS) {
            ESP_LOGE(TAG, "RX result=%d bytes=%" PRIu32, result, size);
            reset_requested = true;
        } else if (!link_valid(&rx, size)) {
            bad_count++;
            ESP_LOGW(TAG, "Invalid frame: %" PRIu32 " bytes", size);
        } else {
            rx_count++;
            last_rx_completed = now_ms();
            if (rx.type <= LINK_HELLO)
                ESP_LOGI(TAG, "RX type=%u seq=%" PRIu32 " reply=%" PRIu32 " text=%.*s",
                         rx.type, rx.seq, rx.reply_to, rx.length, (const char *)rx.payload);
            if (rx.type == LINK_PING) {
                char text[LINK_PAYLOAD_SIZE + 1];
                memcpy(text, rx.payload, rx.length);
                text[rx.length] = 0;
                enqueue(LINK_PONG, rx.seq, text);
            } else if (rx.type == LINK_PONG && rx.reply_to == ping_seq) {
                ESP_LOGI(TAG, "PONG matched seq=%" PRIu32 " RTT=%" PRId64 " ms",
                         ping_seq, now_ms() - ping_sent);
                ping_seq = 0;
            } else if (rx.type == LINK_PACKET) {
                if (ethernet_mode && !ethernet_pending) { ethernet_fragment=rx; ethernet_pending=true; }
                else ++ethernet_bad;
            } else if (rx.type == LINK_WIFI_SCAN_REQUEST && native_wifi) {
                if(native_scan_request_valid(&rx) && (native_profile_busy() || !native_scan_request(&rx,native_epoch))) {
                    link_frame_t busy;
                    native_scan_encode_done(&busy,++seq,rx.seq,0,NATIVE_SCAN_BUSY);
                    if(xQueueSend(outgoing,&busy,0)!=pdTRUE)++dropped;
                }
            } else if(rx.type==LINK_WIFI_PROFILE && native_wifi) {
                unsigned request=rx.reply_to;
                int result=native_profile_receive(&profile_receiver,&rx);
                profile_received_at=now_ms();
                if(result==2 || result<0) {
                    int accepted=result==2 && !native_scan_busy() && native_profile_submit(&profile_receiver.profile,request,native_epoch);
                    native_profile_clear(&profile_receiver,sizeof(profile_receiver));
                    if(!accepted && request) {
                        link_frame_t reply;link_init(&reply,LINK_WIFI_PROFILE_RESULT,++seq,request,NULL);
                        reply.length=8;reply.payload[0]=1;link_put_u32(reply.payload+4,1);
                        if(xQueueSend(outgoing,&reply,0)!=pdTRUE)++dropped;
                    }
                }
                native_profile_clear(&rx,sizeof(rx));
            } else if (rx.type == LINK_HELLO) {
                ++native_epoch;native_profile_clear(&profile_receiver,sizeof(profile_receiver));
                native_wifi=rx.length==sizeof(LINK_NATIVE_HELLO)-1 && !memcmp(rx.payload,LINK_NATIVE_HELLO,rx.length);
                ethernet_metadata=native_wifi || (rx.length==sizeof(LINK_BROWSER_HELLO)-1 && !memcmp(rx.payload,LINK_BROWSER_HELLO,rx.length));
                ethernet_network=ethernet_metadata || (rx.length==sizeof(LINK_NET_HELLO)-1 && !memcmp(rx.payload,LINK_NET_HELLO,rx.length));
                ethernet_mode = ethernet_network || (rx.length == sizeof(LINK_PACKET_HELLO)-1 &&
                    !memcmp(rx.payload, LINK_PACKET_HELLO, rx.length));
                ethernet_rx.total=ethernet_tx.total=0;
                ethernet_pending=false;
                router_link_pending=true;
                next_net_status=0;
                if (ethernet_mode) {
                    enqueue(LINK_HELLO, rx.seq, native_wifi ? LINK_NATIVE_HELLO : ethernet_metadata ? LINK_BROWSER_HELLO : ethernet_network ? LINK_NET_HELLO : LINK_PACKET_HELLO);
                    ESP_LOGI(TAG, "Ethernet USB lab capability ready (ESP local ARP/UDP fixture)");
                }
                html_receiver = rx.length == sizeof(LINK_HTML_HELLO) - 1 &&
                    !memcmp(rx.payload, LINK_HTML_HELLO, sizeof(LINK_HTML_HELLO) - 1);
                if (html_receiver) network_page_request();
                ESP_LOGI(TAG, "PSP HTML capability=%d; v5 receiver required for file save", html_receiver);
            } else if (rx.type == LINK_GET_PAGE) {
                network_page_request();
                if (!file_active) file_wanted = true;
                ESP_LOGI(TAG, "PSP requested page; cached=%d active=%d", page.data != NULL, file_active);
            } else if (rx.type == LINK_FILE_ACK || rx.type == LINK_FILE_ERROR) {
                file_acknowledged(&rx);
            }
        }
    }
    return true;
}

static const usbh_class_driver_t link_driver = {
    .name = "PSP WiFi Link", .init = class_init, .deinit = class_deinit,
    .open = class_open, .set_config = class_configure,
    .xfer_cb = class_transfer, .close = class_close,
};

const usbh_class_driver_t *usbh_app_driver_get_cb(uint8_t *count)
{
    *count = 1;
    return &link_driver;
}

static void auth_complete(tuh_xfer_t *transfer)
{
    auth_busy = false;
    bool ok=transfer->result == XFER_RESULT_SUCCESS && transfer->actual_len == 1 && auth_data[0] == 0;
    ESP_LOGI(TAG, "Accessory handshake attempt=%u result=%d bytes=%" PRIu32 " first=%02x",
             auth_attempts, transfer->result, transfer->actual_len,
             transfer->actual_len ? auth_data[0] : 0xff);
    if (ok || auth_attempts >= 3) {
        auth_pending=false;
        if (!ok) ESP_LOGW(TAG,"Accessory not authenticated; trying custom pipe after retries");
    } else auth_due=now_ms()+100;

}

void tuh_mount_cb(uint8_t address)
{
    if (address != device) {
        ESP_LOGW(TAG, "Device has no PSP link interface; launch PSP ESP Link, not accpower");
        return;
    }
    mounted = true;
    ESP_LOGI(TAG, "LINK READY; standard enumeration completed (no camera-driver patch)");
    auth_request = (tusb_control_request_t){
        .bmRequestType = 0xc1, .bRequest = 1, .wValue = 3, .wIndex = 0, .wLength = 1,
    };
    /* The mount event can precede PSP-side accessory readiness. Do not race
     * bulk OUT against the initial control exchange; retry a negative reply. */
    auth_pending=true;
    auth_busy=false;
    auth_attempts=0;
    auth_due=now_ms()+100;
    enqueue(LINK_HELLO, 0, "hello from ESP32-S3");
}

void tuh_umount_cb(uint8_t address) { class_close(address); }

static void print_status(void)
{
    ESP_LOGI(TAG, "STATUS mounted=%d addr=%u itf=%u GPIO4=%d auto=%d TX=%" PRIu32
             " RX=%" PRIu32 " invalid=%" PRIu32 " dropped=%" PRIu32 " pending(in/out)=%d/%d auth=%d",
             mounted, device, itf_number, vbus_on, automatic,
             tx_count, rx_count, bad_count, dropped, rx_busy, tx_busy, auth_busy);
    ESP_LOGI(TAG, "USB PHY host=%d port_connected=%d events(attach/remove/xfer)=%" PRIu32
             "/%" PRIu32 "/%" PRIu32,
             tuh_inited(), tuh_inited() ? hcd_port_connect_status(0) : 0,
             attach_events, remove_events, transfer_events);
    ESP_LOGI(TAG, "ETHERNET mode=%d packets_in=%" PRIu32 " replies=%" PRIu32 " bad=%" PRIu32,
             ethernet_mode, ethernet_received, ethernet_replied, ethernet_bad);
    ESP_LOGI(TAG, "SERVICE event_wakeup=1 tick_ms=%u watchdog_resets=%" PRIu32,
             (unsigned)portTICK_PERIOD_MS, watchdog_resets);
    usb_router_status();
    native_scan_status();
    network_page_status();
    ESP_LOGI(TAG, "PAGE cached=%lu bytes HTML_receiver=%d sending=%d offset=%lu",
             (unsigned long)page.size, html_receiver, file_active, (unsigned long)file_offset);
}

static void file_prepare(unsigned stage)
{
    file_stage = stage;
    unsigned type = stage == 0 ? LINK_FILE_BEGIN : stage == 1 ? LINK_FILE_DATA : LINK_FILE_END;
    link_init(&file_request, type, ++seq, file_session, NULL);
    if (stage == 0) {
        file_session = file_request.seq;
        file_request.reply_to = 0;
    }
    if (stage == 1) {
        unsigned count = page.size - file_offset;
        if (count > LINK_FILE_CHUNK) count = LINK_FILE_CHUNK;
        link_put_u32(file_request.payload, file_offset);
        memcpy(file_request.payload + 4, page.data + file_offset, count);
        file_request.length = 4 + count;
        file_expected_offset = file_offset + count;
    } else {
        link_put_u32(file_request.payload, page.size);
        link_put_u32(file_request.payload + 4, page.crc);
        file_request.length = 8;
        file_expected_offset = stage == 0 ? 0 : page.size;
    }
    file_waiting = false;
    file_retries = 0;
}

static void file_failed(const char *reason)
{
    ESP_LOGE(TAG, "FILE failed at %lu/%lu: %s (TRIANGLE retries cached page)",
             (unsigned long)file_offset, (unsigned long)page.size, reason);
    link_frame_t abort_frame;
    link_init(&abort_frame, LINK_FILE_ABORT, ++seq, file_session, NULL);
    xQueueSend(outgoing, &abort_frame, 0);
    file_active = file_waiting = file_wanted = false;
}

static void file_acknowledged(const link_frame_t *ack)
{
    if (!file_active || !file_waiting || ack->reply_to != file_request.seq) return;
    if (ack->type == LINK_FILE_ERROR) {
        ESP_LOGE(TAG, "PSP file error code=%lu", ack->length >= 8 ?
                 (unsigned long)link_get_u32(ack->payload + 4) : 0ul);
        file_failed("PSP receiver rejected request");
        return;
    }
    if (ack->length != 8 || link_get_u32(ack->payload) != file_expected_offset ||
        link_get_u32(ack->payload + 4) != 0) {
        file_failed("invalid ACK offset/status");
        return;
    }
    file_offset = file_expected_offset;
    if (file_stage == 2) {
        ESP_LOGI(TAG, "FILE COMPLETE: PSP committed ms0:/pspdev.html bytes=%lu CRC32=%08lx",
                 (unsigned long)page.size, (unsigned long)page.crc);
        file_active = file_waiting = file_wanted = false;
    } else file_prepare(file_offset < page.size ? 1 : 2);
}

static void file_pump(int64_t now)
{
    if (!page.data && network_page_take(&page))
        ESP_LOGI(TAG, "Cached HTML ready for PSP: %lu bytes", (unsigned long)page.size);
    if (!mounted || !html_receiver || !page.data) return;
    if (!file_active && file_wanted) {
        file_active = true;
        file_offset = 0;
        file_prepare(0);
        ESP_LOGI(TAG, "FILE sending %lu bytes CRC32=%08lx to PSP", (unsigned long)page.size,
                 (unsigned long)page.crc);
    }
    if (!file_active) return;
    if (!file_waiting || now >= file_deadline) {
        if (file_waiting && file_retries >= 3) {
            file_failed("ACK timeout after 3 retries");
            return;
        }
        if (xQueueSend(outgoing, &file_request, 0) == pdTRUE) {
            if (file_waiting) {
                file_retries++;
                ESP_LOGW(TAG, "FILE retry seq=%lu attempt=%u", (unsigned long)file_request.seq, file_retries);
            }
            file_waiting = true;
            file_deadline = now + 2000;
        }
    }
}

static void ethernet_pump(void)
{
    if (!ethernet_mode || !ethernet_pending) return;
    ethernet_pending=false;
    int n=packet_link_receive(&ethernet_rx,&ethernet_fragment);
    if(n<0) { ++ethernet_bad; return; }
    if(!n) return;
    ++ethernet_received;
    /* Real lwIP handles ARP (and learns the PSP MAC). Preserve the UDP fixture
     * as a regression service; all other IPv4 packets enter forwarding/NAPT. */
    size_t reply=0;
    if (ethernet_rx.data[12]==8 && ethernet_rx.data[13]==0)
        reply=vnet_lab_reply(ethernet_rx.data,n,ethernet_reply,sizeof(ethernet_reply));
    if (reply) {
        if (packet_link_begin(&ethernet_tx,ethernet_reply,reply,++ethernet_id)) ++ethernet_replied;
        else ++ethernet_bad;
    } else if (!usb_router_receive(ethernet_rx.data,n)) ++ethernet_bad;
    if(ethernet_received<=12) ESP_LOGI(TAG,"ETHERNET RX packet=%" PRIu32 " bytes=%d fixture_reply=%u bad=%" PRIu32,
             ethernet_received,n,(unsigned)reply,ethernet_bad);
}

static void host_stop(void)
{
    if (!tuh_deinit(0)) {
        ESP_LOGE(TAG, "Host deinit failed; restarting rather than reusing live buffers");
        abort();
    }
    native_wifi=false;++native_epoch;native_profile_clear(&profile_receiver,sizeof(profile_receiver));
    ethernet_mode = ethernet_pending = ethernet_network = ethernet_metadata = false;
    router_link_pending=true;
    ethernet_rx.total = ethernet_tx.total = 0;
    mounted = rx_busy = tx_busy = auth_busy = auth_pending = false;
    html_receiver = file_active = file_waiting = false;
    file_wanted = true;
    device = candidate = ep_in = ep_out = 0;
    ping_seq = 0;
    xQueueReset(outgoing);
}

static void handle_command(const char *line)
{
    if (!strcmp(line, "help")) {
        ESP_LOGI(TAG, "Commands: status | ping [text] | say <text> | auto on/off | vbus on/off | reset | help");
    } else if (!strcmp(line, "status")) print_status();
    else if (!strcmp(line, "auto on")) automatic = true;
    else if (!strcmp(line, "auto off")) automatic = false;
    else if (!strcmp(line, "vbus off")) {
        set_vbus(false);
        host_stop();
        reset_requested = false;
    }
    else if (!strcmp(line, "vbus on")) { set_vbus(true); reset_requested = true; }
    else if (!strcmp(line, "reset")) reset_requested = true;
    else if (!strcmp(line, "ping") || !strncmp(line, "ping ", 5)) {
        if (mounted) enqueue(LINK_PING, 0, line[4] ? line + 5 : "UART ping");
        else ESP_LOGW(TAG, "No link; packet not queued");
    } else if (!strncmp(line, "say ", 4)) {
        if (mounted) enqueue(LINK_TEXT, 0, line + 4);
        else ESP_LOGW(TAG, "No link; packet not queued");
    } else ESP_LOGW(TAG, "Unknown command: %s (type help)", line);
}

static void uart_console(void *arg)
{
    command_t command = {0};
    unsigned used = 0;
    bool overflow = false;
    (void)arg;
    for (;;) {
        uint8_t ch;
        if (uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (ch == '\r' || ch == '\n') {
            if (overflow) ESP_LOGW(TAG, "Command too long; discarded");
            else if (used) {
                command.text[used] = 0;
                if (xQueueSend(commands, &command, 0) != pdTRUE)
                    ESP_LOGW(TAG, "Command queue full");
                else host_wakeup_signal(false);
            }
            used = 0;
            overflow = false;
        } else if (ch == 8 || ch == 127) {
            if (used) used--;
        } else if (ch >= 32 && ch < 127) {
            if (used + 1 < sizeof(command.text)) command.text[used++] = ch;
            else overflow = true;
        }
    }
}

static void host_start(void)
{
    const tusb_rhport_init_t init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_FULL};
    if (!tusb_rhport_init(0, &init)) {
        ESP_LOGE(TAG, "Host init failed");
        abort();
    }
}

void app_main(void)
{
    host_wakeup_bind();
    ESP_LOGI(TAG,"POWER boot reset_reason=%d CPU=%d MHz (brownout reason=%d)",
             esp_reset_reason(),CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,ESP_RST_BROWNOUT);
    gpio_config_t gpio = {.pin_bit_mask = 1ULL << VBUS_GPIO, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&gpio));
    set_vbus(false);
    const uart_config_t uart = {
        .baud_rate = 115200, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
    commands = xQueueCreate(8, sizeof(command_t));
    outgoing = xQueueCreate(8, sizeof(link_frame_t));
    configASSERT(commands && outgoing);
    configASSERT(xTaskCreate(uart_console, "link_uart", 4096, NULL, 4, NULL) == pdPASS);
    usb_phy_handle_t phy;
    const usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG, .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_HOST, .otg_speed = USB_PHY_SPEED_FULL,
    };
    ESP_ERROR_CHECK(usb_new_phy(&phy_config, &phy));
    host_start();
    set_vbus(true);
    network_page_start();
    native_scan_start();
    native_profile_start();
    usb_router_start();
    local_tcp_start();
    ESP_LOGI(TAG, "PSP link host v15 native scan diagnostics; runtime credentials. UART0 115200; type help. D+=20 D-=19");
    ESP_LOGI(TAG, "Two-cable setup: connector VBUS is external; GPIO4 does not switch its 5V");
    int64_t next_ping = now_ms() + 2000;
    int64_t next_status = now_ms() + 10000;
    for (;;) {
        tuh_task_ext(0, false);
        if (router_link_pending) {
            router_link_pending=false;
            usb_router_link(false);
            if (mounted && ethernet_mode) usb_router_link(true);
        }
        ethernet_pump();
        /* Drain stale transactions even after detach so the service can accept
         * a request from the next session. Never drop a current result to make
         * room: the bounded result queue provides backpressure. */
        if(!mounted || !native_wifi || uxQueueSpacesAvailable(outgoing)) {
            link_frame_t reply;unsigned epoch;
            if(native_scan_take(&reply,&epoch) && mounted && native_wifi && epoch==native_epoch) {
                reply.seq=++seq;
                configASSERT(xQueueSend(outgoing,&reply,0)==pdTRUE);
            }
        }
        if (mounted && ethernet_mode && !ethernet_tx.total) {
            unsigned n=usb_router_take(ethernet_reply,sizeof(ethernet_reply));
            if(n) packet_link_begin(&ethernet_tx,ethernet_reply,n,++ethernet_id);
        }
        if(!mounted || !native_wifi || uxQueueSpacesAvailable(outgoing)) {
            link_frame_t reply;unsigned epoch;
            if(native_profile_take(&reply,&epoch) && mounted && native_wifi && epoch==native_epoch) {
                reply.seq=++seq;configASSERT(xQueueSend(outgoing,&reply,0)==pdTRUE);
            }
        }
        if(profile_receiver.request && now_ms()-profile_received_at>5000)
            native_profile_clear(&profile_receiver,sizeof(profile_receiver));
        command_t command;
        if (xQueueReceive(commands, &command, 0) == pdTRUE) handle_command(command.text);
        int64_t now = now_ms();
        if (reset_requested) {
            reset_requested = false;
            set_vbus(false);
            /* Deinit aborts transfers before static buffers are reused. */
            host_stop();
            vTaskDelay(pdMS_TO_TICKS(300));
            host_start();
            set_vbus(true);
            next_ping = now_ms() + 2000;
        }
        if (mounted) {
            if (ethernet_network && now>=next_net_status) {
                link_frame_t info; uint8_t ip[4]; unsigned state=network_page_uplink(ip);
                link_init(&info,LINK_NET_STATUS,++seq,0,NULL);
                info.length=8; link_put_u32(info.payload,state); memcpy(info.payload+4,ip,4);
                if(xQueueSend(outgoing,&info,0)!=pdTRUE) ++dropped;
                if(ethernet_metadata) {
                    link_init(&info,LINK_WIFI_INFO,++seq,0,NULL);info.length=48;
                    network_page_wifi_info(info.payload);
                    if(xQueueSend(outgoing,&info,0)!=pdTRUE)++dropped;
                }
                next_net_status=now+2000;
            }
            if (auth_pending && !auth_busy && now >= auth_due) {
                memset(auth_data,0,sizeof(auth_data));
                tuh_xfer_t transfer={.daddr=device, .ep_addr=0, .setup=&auth_request,
                    .buffer=auth_data, .complete_cb=auth_complete};
                ++auth_attempts;
                auth_busy=tuh_control_xfer(&transfer);
                if (!auth_busy) {
                    ESP_LOGW(TAG,"Accessory request submission failed attempt=%u",auth_attempts);
                    if(auth_attempts>=3) auth_pending=false;
                    else auth_due=now+100;
                }
            }
            if (!rx_busy) {
                rx_busy = usbh_edpt_xfer(device, ep_in, (uint8_t *)&rx, sizeof(rx));
                if (!rx_busy) reset_requested = true;
            }
            if (automatic && !ethernet_mode && !file_active && now >= next_ping) {
                enqueue(LINK_PING, 0, "ESP heartbeat");
                next_ping = now + 2000;
            }
            bool have_tx=false;
            if (!tx_busy && !auth_pending && !auth_busy) {
                have_tx=xQueueReceive(outgoing, &tx, 0)==pdTRUE;
                if(!have_tx && ethernet_mode) have_tx=packet_link_next(&ethernet_tx,&tx,++seq);
            }
            if (have_tx) {
                tx_busy = usbh_edpt_xfer(device, ep_out, (uint8_t *)&tx, sizeof(tx));
                tx_started = now;
                if (!tx_busy) reset_requested = true;
            }
            if (link_tx_stalled(tx_busy, now - tx_started, TRANSFER_WATCHDOG_MS)) {
                ++watchdog_resets;
                ESP_LOGW(TAG, "Transfer watchdog: restarting host type=%u seq=%" PRIu32
                         " pending_ms=%" PRId64 " last_rx_ms=%" PRId64 " last_tx_ms=%" PRId64
                         " packet=%" PRIu32 " offset=%" PRIu32 "/%" PRIu32,
                         tx.type, tx.seq, now-tx_started, now-last_rx_completed,
                         now-last_tx_completed, ethernet_tx.id, ethernet_tx.offset, ethernet_tx.total);
                reset_requested = true;
            }
        }
        file_pump(now);
        if (now >= next_status) { print_status(); next_status = now + 10000; }
        host_wakeup_wait();
    }
}
