#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "link_protocol.h"
#include "network_page.h"

#include "lwip/netdb.h"

static const char *TAG = "page-net";
static EventGroupHandle_t events;
static QueueHandle_t completed;
static esp_netif_t *station;
static unsigned retries;
static volatile bool configured,radio_running;
#define RADIO_READY BIT3
#define RADIO_START_DELAY_MS 3000
/* Normal externally powered profile; SDK units are quarter dBm. */
#define RADIO_TX_POWER_QDBM 80
#define GOT_IP BIT0
#define FAILED BIT1
#define PAGE_REQUEST BIT2
static portMUX_TYPE remote_lock=portMUX_INITIALIZER_UNLOCKED;
static uint8_t remote_ip[4];
static uint8_t configured_dns[2][4];
static bool manual_dns;

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        radio_running=true;
        /* Apply before the first scan/association, including after a restart.
         * PHY init data separately caps power before this API is available. */
        ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(RADIO_TX_POWER_QDBM));
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
        ESP_LOGI(TAG, "POWER radio ready: TX cap=20 dBm, modem sleep disabled (external supply)");
        if(configured)esp_wifi_connect();
    }
    else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_STOP){radio_running=false;}
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = data;
        xEventGroupClearBits(events, GOT_IP);
        portENTER_CRITICAL(&remote_lock); memset(remote_ip,0,4); portEXIT_CRITICAL(&remote_lock);
        ESP_LOGW(TAG, "Wi-Fi disconnected reason=%u retry=%u", event->reason, retries);
        if(configured && radio_running) {
            if (retries++ < 8) esp_wifi_connect();
            else xEventGroupSetBits(events, FAILED);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        if(!configured || !radio_running)return;
        uint8_t dns[2][4];bool custom;
        portENTER_CRITICAL(&remote_lock);custom=manual_dns;memcpy(dns,configured_dns,sizeof(dns));portEXIT_CRITICAL(&remote_lock);
        if(custom)for(unsigned i=0;i<2;++i) {
            esp_netif_dns_info_t info={0};info.ip.type=ESP_IPADDR_TYPE_V4;
            memcpy(&info.ip.u_addr.ip4.addr,dns[i],4);
            if(esp_netif_set_dns_info(station,i?ESP_NETIF_DNS_BACKUP:ESP_NETIF_DNS_MAIN,&info)!=ESP_OK) {
                xEventGroupSetBits(events,FAILED);return;
            }
        }
        retries = 0;
        ESP_LOGI(TAG, "Wi-Fi connected; IP=" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(events, GOT_IP);
    }
}

void network_page_status(void)
{
    int8_t power=0;
    wifi_ps_type_t ps;
    if (esp_wifi_get_max_tx_power(&power)==ESP_OK && esp_wifi_get_ps(&ps)==ESP_OK)
        ESP_LOGI(TAG,"POWER CPU=%d MHz TX_cap_qdBm=%d modem_sleep=%d",
                 CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,power,ps);
    else ESP_LOGI(TAG,"POWER radio waiting for staged startup");
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        esp_netif_ip_info_t ip;
        esp_netif_get_ip_info(station, &ip);
        ESP_LOGI(TAG, "NETWORK connected RSSI=%d IP=" IPSTR, ap.rssi, IP2STR(&ip.ip));
    } else ESP_LOGI(TAG, "NETWORK not connected");
    uint8_t ip[4];unsigned flags=network_page_uplink(ip);
    ESP_LOGI(TAG,"UPLINK flags=%u test_target=%u.%u.%u.%u",flags,ip[0],ip[1],ip[2],ip[3]);
}

unsigned network_page_uplink(uint8_t ip[4])
{
    unsigned flags=(xEventGroupGetBits(events)&GOT_IP)?1:0;
    portENTER_CRITICAL(&remote_lock); memcpy(ip,remote_ip,4); portEXIT_CRITICAL(&remote_lock);
    if (flags && (ip[0]|ip[1]|ip[2]|ip[3])) flags|=2;
    return flags;
}
void network_page_request(void) { xEventGroupSetBits(events,PAGE_REQUEST); }
static void resolve_task(void *unused)
{
    (void)unused;
    for (;;) {
        if (!(xEventGroupGetBits(events)&GOT_IP)) {
            if (configured && radio_running && (xEventGroupGetBits(events)&FAILED)) {
                xEventGroupClearBits(events,FAILED); retries=0; esp_wifi_connect();
            }
        } else {
            uint8_t current[4];
            if (!(network_page_uplink(current)&2)) {
                struct addrinfo hints={.ai_family=AF_INET,.ai_socktype=SOCK_STREAM}, *addresses=NULL;
                int r=getaddrinfo(LINK_TEST_HOST,NULL,&hints,&addresses);
                if (!r && addresses && (xEventGroupGetBits(events)&GOT_IP)) {
                    struct sockaddr_in *a=(void *)addresses->ai_addr;
                    portENTER_CRITICAL(&remote_lock); memcpy(remote_ip,&a->sin_addr,4); portEXIT_CRITICAL(&remote_lock);
                    ESP_LOGI(TAG,"Native PSP test target resolved: " LINK_TEST_HOST);
                }
                if(r) ESP_LOGW(TAG,"Native test DNS lookup failed result=%d; retrying",r);
                if(addresses) freeaddrinfo(addresses);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
static bool fetch_page(network_page_t *page)
{
    const esp_http_client_config_t config = {
        .url = PAGE_URL, .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000, .buffer_size = 4096, .max_redirection_count = 4,
        .user_agent = "PSP-E1000-ESP-Link/5",
    };
    esp_http_client_handle_t http = esp_http_client_init(&config);
    if (!http) return false;
    bool ok = false;
    uint8_t *body = NULL;
    esp_http_client_set_header(http, "Accept-Encoding", "identity");
    esp_err_t error = esp_http_client_open(http, 0);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "HTTPS open: %s", esp_err_to_name(error));
        goto done;
    }
    int64_t advertised = esp_http_client_fetch_headers(http);
    int status = esp_http_client_get_status_code(http);
    ESP_LOGI(TAG, "HTTPS status=%d content_length=%lld", status, (long long)advertised);
    if (status != 200 || advertised < 0 || advertised > LINK_FILE_LIMIT) goto done;
    /* Stream into a bounded buffer, then publish immutable bytes to USB task.
     * Never make the PSP wait for the network between individual file chunks. */
    unsigned capacity = advertised > 0 ? (unsigned)advertised : 16384;
    body = malloc(capacity + 1);
    if (!body) goto done;
    unsigned used = 0;
    for (;;) {
        if (used && esp_http_client_is_complete_data_received(http)) break;
        if (used == capacity) {
            if (capacity == LINK_FILE_LIMIT) {
                if (!esp_http_client_is_complete_data_received(http)) goto done;
                break;
            }
            unsigned larger = capacity * 2;
            if (larger > LINK_FILE_LIMIT) larger = LINK_FILE_LIMIT;
            uint8_t *replacement = realloc(body, larger + 1);
            if (!replacement) goto done;
            body = replacement;
            capacity = larger;
        }
        int n = esp_http_client_read(http, (char *)body + used,
                                     capacity - used > 4096 ? 4096 : capacity - used);
        if (n < 0) goto done;
        if (!n) break;
        used += n;
    }
    if (!used || !esp_http_client_is_complete_data_received(http) ||
        (advertised > 0 && (uint64_t)advertised != used)) goto done;
    body[used] = 0;
    page->data = body;
    page->size = used;
    page->crc = link_crc_update(UINT32_MAX, body, used) ^ UINT32_MAX;
    ESP_LOGI(TAG, "HTTPS READY bytes=%u CRC32=%08lx url=" PAGE_URL,
             used, (unsigned long)page->crc);
    ok = true;
done:
    esp_http_client_close(http);
    esp_http_client_cleanup(http);
    if (!ok) {
        free(body);
        ESP_LOGE(TAG, "Page fetch failed; nothing published to PSP");
    }
    return ok;
}

static void download_task(void *arg)
{
    (void)arg;
    xEventGroupWaitBits(events,PAGE_REQUEST,true,false,portMAX_DELAY);
    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(events, GOT_IP | FAILED, false, false, pdMS_TO_TICKS(60000));
        if (bits & GOT_IP) break;
        ESP_LOGW(TAG, "Waiting for hotspot IP; check hotspot and 2.4GHz availability");
        vTaskDelay(pdMS_TO_TICKS(5000));
        xEventGroupClearBits(events, FAILED);
        retries = 0;
        if(configured && radio_running)esp_wifi_connect();
    }
    esp_sntp_config_t time_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t error = esp_netif_sntp_init(&time_config);
    if (error != ESP_OK || esp_netif_sntp_sync_wait(pdMS_TO_TICKS(60000)) != ESP_OK) {
        ESP_LOGE(TAG, "Time sync failed; refusing HTTPS with an unset certificate clock");
        vTaskDelete(NULL);
        return;
    }
    time_t now;
    time(&now);
    ESP_LOGI(TAG, "SNTP time synchronized epoch=%lld; starting verified HTTPS", (long long)now);
    network_page_t page;
    for (unsigned attempt = 0; attempt < 3; attempt++) {
        if (fetch_page(&page)) {
            xQueueSend(completed, &page, portMAX_DELAY);
            vTaskDelete(NULL);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    vTaskDelete(NULL);
}

bool network_page_take(network_page_t *page)
{
    return xQueueReceive(completed, page, 0) == pdTRUE;
}

static void radio_start_task(void *unused)
{
    (void)unused;
    ESP_LOGI(TAG, "POWER radio held off for %u ms; USB service continues", RADIO_START_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(RADIO_START_DELAY_MS));
    ESP_LOGI(TAG, "POWER starting radio (PHY ceiling=%d dBm)", CONFIG_ESP_PHY_MAX_WIFI_TX_POWER);
    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupSetBits(events,RADIO_READY);
    vTaskDelete(NULL);
}

void network_page_start(void)
{
    events = xEventGroupCreate();
    completed = xQueueCreate(1, sizeof(network_page_t));
    configASSERT(events && completed);
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(error);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    station = esp_netif_create_default_wifi_sta();
    configASSERT(station);
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_log_level_set("wifi", ESP_LOG_WARN);
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /* Start an unconfigured station for scanning. PSP profiles are the source
     * of credentials; no compile-time fallback and no automatic NVS restore. */
    wifi_config_t config={0};
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&config));
    configASSERT(xTaskCreate(radio_start_task,"radio_start",4096,NULL,4,NULL)==pdPASS);
    ESP_LOGI(TAG, "Wi-Fi station prepared; waiting for a PSP network profile");
    configASSERT(xTaskCreate(resolve_task,"native_dns",4096,NULL,4,NULL)==pdPASS);
    configASSERT(xTaskCreate(download_task, "page_https", 8192, NULL, 4, NULL) == pdPASS);
}

/* Network capability v2: DNS4+DNS4+SSID32+BSSID6+strength1+channel/auth1.
 * Never send or log credentials. This reports the actual station association. */
void network_page_wifi_info(uint8_t output[48])
{
    memset(output,0,48);
    for(unsigned i=0;i<2;++i) {
        esp_netif_dns_info_t dns;
        if(esp_netif_get_dns_info(station,i?ESP_NETIF_DNS_BACKUP:ESP_NETIF_DNS_MAIN,&dns)==ESP_OK
           && dns.ip.type==ESP_IPADDR_TYPE_V4)memcpy(output+i*4,&dns.ip.u_addr.ip4.addr,4);
    }
    wifi_ap_record_t ap;
    if(esp_wifi_sta_get_ap_info(&ap)==ESP_OK) {
        memcpy(output+8,ap.ssid,32);memcpy(output+40,ap.bssid,6);
        int strength=(ap.rssi+100)*2;if(strength<0)strength=0;if(strength>100)strength=100;
        output[46]=strength;output[47]=(ap.primary&15)|((ap.authmode&15)<<4);
    }
}

/* Called only by the profile worker, never from USB or the Wi-Fi event loop. */
int network_page_apply_profile(const native_wifi_profile_t *profile)
{
    if(!native_profile_valid(profile))return -1;
    if(!(xEventGroupWaitBits(events,RADIO_READY,false,true,pdMS_TO_TICKS(5000))&RADIO_READY))return -1;
    configured=false;radio_running=false;
    esp_err_t error=esp_wifi_stop();
    if(error!=ESP_OK)return -1;
    xEventGroupClearBits(events,GOT_IP|FAILED);
    portENTER_CRITICAL(&remote_lock);memset(remote_ip,0,4);portEXIT_CRITICAL(&remote_lock);
    retries=0;
    portENTER_CRITICAL(&remote_lock);manual_dns=profile->dns_mode!=0;memcpy(configured_dns,profile->dns,sizeof(configured_dns));portEXIT_CRITICAL(&remote_lock);
    error=esp_netif_dhcpc_stop(station);
    if(error!=ESP_OK && error!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED){esp_wifi_start();return -1;}
    esp_netif_ip_info_t address={0};
    if(profile->ip_mode) {
        memcpy(&address.ip.addr,profile->ip,4);memcpy(&address.netmask.addr,profile->mask,4);memcpy(&address.gw.addr,profile->gateway,4);
    }
    if(esp_netif_set_ip_info(station,&address)!=ESP_OK){esp_wifi_start();return -1;}
    if(!profile->ip_mode) {
        error=esp_netif_dhcpc_start(station);
        if(error!=ESP_OK && error!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED){esp_wifi_start();return -1;}
    }
    wifi_config_t config={0};
    memcpy(config.sta.ssid,profile->ssid,profile->ssid_length);
    memcpy(config.sta.password,profile->key,profile->key_length);
    config.sta.threshold.authmode=profile->security==0?WIFI_AUTH_OPEN:
        profile->security<=2?WIFI_AUTH_WEP:WIFI_AUTH_WPA_PSK;
    config.sta.scan_method=WIFI_ALL_CHANNEL_SCAN;
    error=esp_wifi_set_config(WIFI_IF_STA,&config);
    native_profile_clear(&config,sizeof(config));
    if(error!=ESP_OK){esp_wifi_start();return -1;}
    configured=true;
    error=esp_wifi_start();
    if(error!=ESP_OK){configured=false;return -1;}
    ESP_LOGI(TAG,"PSP network profile applied (RAM only)");return 0;
}
