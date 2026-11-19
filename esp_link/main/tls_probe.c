/* Explicit UART diagnostic; never starts during normal PSP browser use. */
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "tls_upstream.h"
static volatile unsigned probing;
static void probe(void *unused)
{
    (void)unused;
    const char *hosts[]={"suckless.org","en.touhouwiki.net","google.com"};
    time_t now;time(&now);
    if(now<1704067200)esp_netif_sntp_sync_wait(pdMS_TO_TICKS(12000));
    time(&now);
    for(unsigned i=0;i<3 && now>=1704067200;++i) {
        esp_tls_t *tls=esp_tls_init();int result=-1,detail=0,flags=0;
        esp_tls_cfg_t config=tls_upstream_config();
        if(tls) {
            result=esp_tls_conn_new_sync(hosts[i],strlen(hosts[i]),443,&config,tls);
            if(result!=1) {
                esp_tls_error_handle_t error;
                if(esp_tls_get_error_handle(tls,&error)==ESP_OK)
                    esp_tls_get_and_clear_last_error(error,&detail,&flags);
            }
        }
        ESP_LOGI("tls-probe","site=%u verified=%d detail=%d flags=%x",i,result==1,detail,flags);
        if(tls)esp_tls_conn_destroy(tls);
    }
    if(now<1704067200)ESP_LOGW("tls-probe","clock unavailable");
    __sync_lock_release(&probing);vTaskDelete(NULL);
}
void tls_upstream_probe(void)
{
    if(__sync_lock_test_and_set(&probing,1))return;
    if(xTaskCreate(probe,"tls_probe",8192,NULL,3,NULL)!=pdPASS)__sync_lock_release(&probing);
}
