/*
 * psp_host - ESP32-S3 plays the PSP camera's role: USB HOST for the PSP.
 *
 * The PSP (running accpower, state A--) is the USB device. This firmware:
 *   1. drives the PSP's VBUS pin the way the camera does (short pulse, gap, on),
 *   2. lets the USB host library enumerate the PSP - with the patched enum driver
 *      (tools/patch_enum.py) the last enumeration stage sends the accessory
 *      handshake C1 01 instead of SET_CONFIGURATION,
 *   3. then sends the remaining camera requests itself, SET_CONFIGURATION last,
 *   4. logs every step with timing, and repeats the whole session in a loop.
 *
 * Written against the ESP-IDF 5.x host API (usb/usb_host.h). If 6.x renamed
 * something, send me the compile errors.
 *
 * Wiring (see README.md): GPIO20=D+, GPIO19=D-, GND common, PSP_VBUS_GPIO ->
 * ~47-100 ohm -> PSP VBUS. Never connect 5 V to the PSP.
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"
#include "usb/usb_host.h"
#include "usb/usb_helpers.h"

#define PSP_VBUS_GPIO     4        /* drives the PSP's VBUS pin through 47-100 ohm */
#define VBUS_FIRST_US     1000     /* camera: ~1 ms on ...                          */
#define VBUS_GAP_US       12000    /* ... ~10-14 ms off, then on for good           */
#define SESSION_WAIT_MS   3000     /* wait this long for the PSP to attach          */
#define STEP_TIMEOUT_MS   300      /* per control request                           */
#define HOLD_MS           10000    /* keep a good session open this long            */
#define DO_STAGE2         1        /* also send the "app streaming" requests        */

static const char *TAG = "pspacc";

typedef struct {
    usb_host_client_handle_t client;
    usb_device_handle_t dev;
    volatile uint8_t new_addr;     /* 0 = nothing attached yet */
    volatile bool gone;
    int64_t t_new;                 /* time of NEW_DEV, us */
} ctx_t;

static ctx_t g;

static int ms_since_new(void)
{
    return (int)((esp_timer_get_time() - g.t_new) / 1000);
}

/* ---------------------------------------------------------------- VBUS --- */

static void vbus(bool on)
{
    gpio_set_level(PSP_VBUS_GPIO, on ? 1 : 0);
}

static void vbus_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PSP_VBUS_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    vbus(false);
}

static void vbus_pattern(void)
{
    vbus(true);
    esp_rom_delay_us(VBUS_FIRST_US);
    vbus(false);
    esp_rom_delay_us(VBUS_GAP_US);
    vbus(true);
}

/* --------------------------------------------------------- host library --- */

static bool enum_filter(const usb_device_desc_t *dev_desc, uint8_t *bConfigurationValue)
{
    (void)dev_desc;
    *bConfigurationValue = 1;
    return true;                   /* enumerate everything */
}

static void lib_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
    }
}

static void client_event_cb(const usb_host_client_event_msg_t *msg, void *arg)
{
    (void)arg;
    switch (msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
        g.t_new = esp_timer_get_time();
        g.gone = false;
        g.new_addr = msg->new_dev.address;
        ESP_LOGI(TAG, "NEW_DEV: PSP enumerated, address %d", msg->new_dev.address);
        break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        g.gone = true;
        ESP_LOGW(TAG, "DEV_GONE at +%d ms: the PSP disconnected", ms_since_new());
        break;
    default:
        break;
    }
}

/* ----------------------------------------------------- control transfers --- */

static void xfer_cb(usb_transfer_t *t)
{
    *(volatile bool *)t->context = true;
}

static const char *status_name(int s)
{
    switch (s) {
    case USB_TRANSFER_STATUS_COMPLETED: return "OK";
    case USB_TRANSFER_STATUS_ERROR:     return "ERROR";
    case USB_TRANSFER_STATUS_TIMED_OUT: return "TIMED_OUT";
    case USB_TRANSFER_STATUS_CANCELED:  return "CANCELED";
    case USB_TRANSFER_STATUS_STALL:     return "STALL";
    case USB_TRANSFER_STATUS_OVERFLOW:  return "OVERFLOW";
    case USB_TRANSFER_STATUS_NO_DEVICE: return "NO_DEVICE";
    default:                            return "?";
    }
}

/* One control transfer on EP0. Returns the transfer status (0 = OK), -1 on setup
 * failure. For IN requests the data is logged. */
static int ctrl(uint8_t bm, uint8_t req, uint16_t val, uint16_t idx,
                const uint8_t *out, uint16_t len, const char *label)
{
    const bool in = (bm & 0x80) != 0;
    const int hdr = (int)sizeof(usb_setup_packet_t);
    const int total = hdr + (in ? usb_round_up_to_mps(len, 64) : len);
    usb_transfer_t *x = NULL;
    volatile bool done = false;
    int64_t t0 = esp_timer_get_time();

    if (usb_host_transfer_alloc(total, 0, &x) != ESP_OK || x == NULL) {
        ESP_LOGE(TAG, "%-34s transfer alloc failed", label);
        return -1;
    }
    usb_setup_packet_t *s = (usb_setup_packet_t *)x->data_buffer;
    s->bmRequestType = bm;
    s->bRequest = req;
    s->wValue = val;
    s->wIndex = idx;
    s->wLength = len;
    if (!in && len && out) {
        memcpy(x->data_buffer + hdr, out, len);
    }
    x->num_bytes = total;
    x->device_handle = g.dev;
    x->bEndpointAddress = 0;
    x->callback = xfer_cb;
    x->context = (void *)&done;

    esp_err_t e = usb_host_transfer_submit_control(g.client, x);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "%-34s submit failed: %s", label, esp_err_to_name(e));
        usb_host_transfer_free(x);
        return -1;
    }
    while (!done && (esp_timer_get_time() - t0) < (int64_t)STEP_TIMEOUT_MS * 1000) {
        usb_host_client_handle_events(g.client, pdMS_TO_TICKS(5));
    }
    if (!done) {
        ESP_LOGE(TAG, "%-34s no completion within %d ms", label, STEP_TIMEOUT_MS);
        return -1;                 /* leak the transfer on purpose: it may still be in flight */
    }

    int status = (int)x->status;
    int got = x->actual_num_bytes - hdr;
    if (status != USB_TRANSFER_STATUS_COMPLETED) {
        ESP_LOGW(TAG, "+%4d ms  %-34s %s", ms_since_new(), label, status_name(status));
    } else if (in) {
        char hex[3 * 16 + 1] = {0};
        for (int i = 0; i < got && i < 16; i++) {
            snprintf(hex + 3 * i, 4, "%02x ", x->data_buffer[hdr + i]);
        }
        ESP_LOGI(TAG, "+%4d ms  %-34s OK, %d byte(s): %s", ms_since_new(), label, got, hex);
    } else {
        ESP_LOGI(TAG, "+%4d ms  %-34s OK", ms_since_new(), label);
    }
    usb_host_transfer_free(x);
    return status;
}

/* ------------------------------------------------------------- session --- */

static void run_session(void)
{
    static const uint8_t info[8] = {0x00, 0x00, 0x01, 0x00, 0x01, 0x10, 0x27, 0x03};
    static const uint8_t two[2] = {0x27, 0x03};
    static const uint8_t four[4] = {0, 0, 0, 0};

    if (usb_host_device_open(g.client, g.new_addr, &g.dev) != ESP_OK) {
        ESP_LOGE(TAG, "device open failed");
        return;
    }
    const usb_device_desc_t *dd = NULL;
    if (usb_host_get_device_descriptor(g.dev, &dd) == ESP_OK && dd) {
        ESP_LOGI(TAG, "device %04x:%04x, bcdDevice %04x", dd->idVendor, dd->idProduct, dd->bcdDevice);
    }

    ESP_LOGI(TAG, "--- handshake (enumeration already sent C1 01 as its last stage) ---");
    ctrl(0xC1, 0x01, 0x0003, 0x0000, NULL, 1, "C1 01 wValue=0003 (repeat)");
    ctrl(0x41, 0x01, 0x0000, 0x0000, info, 8, "41 01 accessory info");
    ctrl(0xC1, 0x03, 0x0000, 0x0003, NULL, 5, "C1 03 wIndex=3");
    ctrl(0xC1, 0x08, 0x0000, 0x0003, NULL, 8, "C1 08 wIndex=3");
    ctrl(0x00, 0x09, 0x0001, 0x0000, NULL, 0, "SET_CONFIGURATION 1");
#if DO_STAGE2
    ESP_LOGI(TAG, "--- stage 2: what the camera sends once the app is streaming ---");
    ctrl(0x41, 0x09, 0x002B, 0x0003, four, 4, "41 09 wValue=002B");
    ctrl(0x01, 0x0B, 0x0001, 0x0002, NULL, 0, "SET_INTERFACE(2, alt 1)");
    ctrl(0x41, 0x09, 0x0009, 0x0003, two, 2, "41 09 wValue=0009");
    ctrl(0x01, 0x0B, 0x0001, 0x0003, NULL, 0, "SET_INTERFACE(3, alt 1)");
#endif
    ESP_LOGI(TAG, "--- sequence finished; holding the session for %d s ---", HOLD_MS / 1000);
}

static void pump(int ms)
{
    int64_t t0 = esp_timer_get_time();
    while ((esp_timer_get_time() - t0) < (int64_t)ms * 1000) {
        usb_host_client_handle_events(g.client, pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    vbus_init();

    usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
#ifdef CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
        .enum_filter_cb = enum_filter,
#endif
    };
    ESP_ERROR_CHECK(usb_host_install(&host_cfg));
    xTaskCreate(lib_task, "usb_lib", 4096, NULL, 5, NULL);

    usb_host_client_config_t client_cfg = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {.client_event_callback = client_event_cb, .callback_arg = NULL},
    };
    ESP_ERROR_CHECK(usb_host_client_register(&client_cfg, &g.client));
    ESP_LOGI(TAG, "psp_host ready: VBUS on GPIO%d. Run accpower on the PSP first.", PSP_VBUS_GPIO);

    for (int attempt = 1;; attempt++) {
        ESP_LOGI(TAG, "===== attempt %d: VBUS off, then camera-style power-up =====", attempt);
        vbus(false);
        pump(500);
        g.new_addr = 0;
        g.gone = false;

        vbus_pattern();
        int64_t t0 = esp_timer_get_time();
        while (!g.new_addr && (esp_timer_get_time() - t0) < (int64_t)SESSION_WAIT_MS * 1000) {
            usb_host_client_handle_events(g.client, pdMS_TO_TICKS(10));
        }
        if (!g.new_addr) {
            ESP_LOGW(TAG, "no PSP attached within %d ms (is accpower running and in state A--?)", SESSION_WAIT_MS);
            continue;
        }

        run_session();

        int64_t th = esp_timer_get_time();
        while (!g.gone && (esp_timer_get_time() - th) < (int64_t)HOLD_MS * 1000) {
            usb_host_client_handle_events(g.client, pdMS_TO_TICKS(20));
        }
        if (g.dev) {
            usb_host_device_close(g.client, g.dev);
            g.dev = NULL;
        }
        vbus(false);
        pump(2000);
    }
}
