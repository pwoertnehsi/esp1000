#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "host_wakeup.h"
#include "native_wifi_scan.h"
#include "native_scan.h"
typedef struct {unsigned request,epoch,limit,hidden;} scan_command_t;
typedef struct {link_frame_t frame;unsigned epoch;} scan_reply_t;
static QueueHandle_t requests,replies;
/* active belongs to app_main (request/take); worker owns only the record buffer. */
static int active;
static portMUX_TYPE diagnostic_lock=portMUX_INITIALIZER_UNLOCKED;
static unsigned last_request,last_count,last_result,last_stage;
static esp_err_t last_error;
static void diagnostic(unsigned request,unsigned stage,unsigned result,esp_err_t error,unsigned count)
{
    portENTER_CRITICAL(&diagnostic_lock);
    last_request=request;last_stage=stage;last_result=result;last_error=error;last_count=count;
    portEXIT_CRITICAL(&diagnostic_lock);
}
void native_scan_status(void)
{
    unsigned request,stage,result,count;esp_err_t error;
    portENTER_CRITICAL(&diagnostic_lock);
    request=last_request;stage=last_stage;result=last_result;count=last_count;error=last_error;
    portEXIT_CRITICAL(&diagnostic_lock);
    ESP_LOGI("native-wifi","SCAN request=%u stage=%u result=%u sdk=%d count=%u",request,stage,result,(int)error,count);
}
static wifi_ap_record_t records[NATIVE_SCAN_LIMIT];
static unsigned wire_auth(wifi_auth_mode_t auth)
{
    switch(auth) {
    case WIFI_AUTH_OPEN:return NATIVE_AUTH_OPEN;
    case WIFI_AUTH_WEP:return NATIVE_AUTH_WEP;
    case WIFI_AUTH_WPA_PSK:return NATIVE_AUTH_WPA;
    case WIFI_AUTH_WPA2_PSK:return NATIVE_AUTH_WPA2;
    case WIFI_AUTH_WPA_WPA2_PSK:return NATIVE_AUTH_WPA_MIXED;
    case WIFI_AUTH_WPA3_PSK:return NATIVE_AUTH_WPA3;
    case WIFI_AUTH_WPA2_WPA3_PSK:return NATIVE_AUTH_WPA3_MIXED;
    case WIFI_AUTH_WPA2_ENTERPRISE:return NATIVE_AUTH_ENTERPRISE;
    default:return NATIVE_AUTH_UNKNOWN;
    }
}
static unsigned wire_cipher(wifi_cipher_type_t cipher)
{
    switch(cipher) {
    case WIFI_CIPHER_TYPE_NONE:return NATIVE_CIPHER_NONE;
    case WIFI_CIPHER_TYPE_WEP40:return NATIVE_CIPHER_WEP40;
    case WIFI_CIPHER_TYPE_WEP104:return NATIVE_CIPHER_WEP104;
    case WIFI_CIPHER_TYPE_TKIP:return NATIVE_CIPHER_TKIP;
    case WIFI_CIPHER_TYPE_CCMP:return NATIVE_CIPHER_AES;
    default:return NATIVE_CIPHER_UNKNOWN;
    }
}
static void publish(scan_reply_t *reply)
{
    /* Exactly limit+1 replies fit even if no USB consumer is currently present.
     * A second request is refused until DONE is consumed by the owner thread. */
    configASSERT(xQueueSend(replies,reply,portMAX_DELAY)==pdTRUE);
    host_wakeup_signal(false);
}
static void scan_worker(void *unused)
{
    (void)unused;scan_command_t request;
    for(;;) {
        if(xQueueReceive(requests,&request,portMAX_DELAY)!=pdTRUE)continue;
        wifi_scan_config_t config={.show_hidden=request.hidden!=0};
        diagnostic(request.request,1,0,ESP_OK,0);
        ESP_LOGI("native-wifi","Scan requested id=%u limit=%u",request.request,request.limit);
        esp_err_t error=esp_wifi_scan_start(&config,true);
        unsigned status=error==ESP_OK?NATIVE_SCAN_OK:
            error==ESP_ERR_WIFI_NOT_INIT || error==ESP_ERR_WIFI_NOT_STARTED?NATIVE_SCAN_NOT_READY:
            error==ESP_ERR_WIFI_STATE?NATIVE_SCAN_BUSY:NATIVE_SCAN_FAILED;
        uint16_t count=request.limit;
        unsigned stage=1;
        if(!status) {
            stage=2;error=esp_wifi_scan_get_ap_records(&count,records);
            if(error!=ESP_OK){esp_wifi_clear_ap_list();status=NATIVE_SCAN_FAILED;count=0;}
        }
        if(status)count=0;
        scan_reply_t reply;memset(&reply,0,sizeof(reply));reply.epoch=request.epoch;
        for(unsigned i=0;i<count;++i) {
            native_scan_row_t row;memset(&row,0,sizeof(row));
            row.ssid_length=strnlen((const char *)records[i].ssid,32);
            memcpy(row.ssid,records[i].ssid,row.ssid_length);memcpy(row.bssid,records[i].bssid,6);
            row.channel=records[i].primary;row.rssi=records[i].rssi;
            row.auth=wire_auth(records[i].authmode);row.cipher=wire_cipher(records[i].pairwise_cipher);
            if(!native_scan_encode_row(&reply.frame,0,request.request,i,&row)){stage=3;error=ESP_ERR_INVALID_ARG;status=NATIVE_SCAN_FAILED;break;}
            publish(&reply);
        }
        diagnostic(request.request,stage,status,error,status?0:count);
        native_scan_status();
        native_scan_encode_done(&reply.frame,0,request.request,status?0:count,status);publish(&reply);
        memset(records,0,sizeof(records));
    }
}
void native_scan_start(void)
{
    requests=xQueueCreate(1,sizeof(scan_command_t));replies=xQueueCreate(NATIVE_SCAN_LIMIT+1,sizeof(scan_reply_t));
    configASSERT(requests && replies);
    configASSERT(xTaskCreate(scan_worker,"native_wifi_scan",4096,NULL,3,NULL)==pdPASS);
}
int native_scan_busy(void){return active;}
int native_scan_request(const link_frame_t *frame,unsigned epoch)
{
    if(!requests || active || !native_scan_request_valid(frame))return 0;
    scan_command_t request={frame->seq,epoch,frame->payload[1],frame->payload[2]};
    if(xQueueSend(requests,&request,0)!=pdTRUE)return 0;
    active=1;return 1;
}
int native_scan_take(link_frame_t *frame,unsigned *epoch)
{
    scan_reply_t reply;
    if(!replies || xQueueReceive(replies,&reply,0)!=pdTRUE)return 0;
    *frame=reply.frame;*epoch=reply.epoch;
    if(frame->payload[0]==NATIVE_SCAN_DONE)active=0;
    return 1;
}
