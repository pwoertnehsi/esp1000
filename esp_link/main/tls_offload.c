/* Verified modern TLS on the Wi-Fi side, plaintext only on the private USB LAN. */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "tls_offload.h"
static SemaphoreHandle_t capacity;
static volatile int usb_link;
static volatile TickType_t down_at;
void tls_offload_start(void);
void tls_offload_link(int linked)
{if(!linked && usb_link)down_at=xTaskGetTickCount();usb_link=linked;}
static int exact(int fd,void *buffer,unsigned size,int writing)
{
    unsigned at=0;while(at<size) {
        int n=writing?send(fd,(char *)buffer+at,size-at,0):recv(fd,(char *)buffer+at,size-at,0);
        if(n<=0)return -1;
        at+=n;
    }return 0;
}
static int again(int n)
{return n==ESP_TLS_ERR_SSL_WANT_READ || n==ESP_TLS_ERR_SSL_WANT_WRITE;}
static void serve(void *arg)
{
    int fd=(int)(intptr_t)arg,status=-1,detail=0,cert_flags=0;esp_tls_t *tls=NULL;
    uint8_t header[TLS_OFFLOAD_REPLY];char host[TLS_OFFLOAD_HOST_MAX+1];
    struct timeval timeout={.tv_sec=15};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    if(exact(fd,header,8,0) || !tls_offload_request_valid(header))goto done;
    unsigned length=header[4]|(unsigned)header[5]<<8;
    if(exact(fd,host,length,0) || !tls_offload_host(host,length))goto done;
    host[length]=0;
    time_t now;time(&now);
    if(now<1704067200 && esp_netif_sntp_sync_wait(pdMS_TO_TICKS(12000))!=ESP_OK){status=-3;goto reply;}
    time(&now);if(now<1704067200){status=-3;goto reply;}
    tls=esp_tls_init();if(!tls){status=-4;goto reply;}
    static const char *alpn[]={"http/1.1",NULL};
    esp_tls_cfg_t config={.crt_bundle_attach=esp_crt_bundle_attach,.timeout_ms=12000,.alpn_protos=alpn};
    /* hostname is passed unchanged: SNI and certificate-name verification use
     * the original URL identity. No skip_common_name or insecure fallback. */
    if(esp_tls_conn_new_sync(host,length,443,&config,tls)!=1){
        esp_tls_error_handle_t handle;
        if(esp_tls_get_error_handle(tls,&handle)==ESP_OK)esp_tls_get_and_clear_last_error(handle,&detail,&cert_flags);
        status=-5;goto reply;
    }
    status=0;
reply:
    tls_offload_reply_details(header,status,detail,(uint32_t)cert_flags);
    if(exact(fd,header,TLS_OFFLOAD_REPLY,1) || status)goto done;
    int upstream=-1;if(esp_tls_get_conn_sockfd(tls,&upstream)!=ESP_OK || upstream<0)goto done;
    fcntl(upstream,F_SETFL,fcntl(upstream,F_GETFL,0)|O_NONBLOCK);
    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0)|O_NONBLOCK);
    uint8_t to_tls[1024],to_psp[1024];unsigned out_at=0,out_len=0,in_at=0,in_len=0;int eof=0;
    TickType_t progress=xTaskGetTickCount();
    for(;;) {
        int advanced=0,n;
        if(!usb_link && (TickType_t)(xTaskGetTickCount()-down_at)>pdMS_TO_TICKS(10000))break;
        if(!out_len) {
            n=recv(fd,to_tls,sizeof(to_tls),MSG_DONTWAIT);
            if(n==0)break;
            if(n>0){out_len=n;out_at=0;advanced=1;}
            else if(errno!=EAGAIN && errno!=EWOULDBLOCK)break;
        }
        if(out_len) {
            n=esp_tls_conn_write(tls,to_tls+out_at,out_len);
            if(n>0){out_at+=n;out_len-=n;advanced=1;}
            else if(!again(n))break;
        }
        if(!in_len && !eof) {
            n=esp_tls_conn_read(tls,to_psp,sizeof(to_psp));
            if(n>0){in_len=n;in_at=0;advanced=1;}
            else if(n==0)eof=1;
            else if(!again(n))break;
        }
        if(in_len) {
            n=send(fd,to_psp+in_at,in_len,MSG_DONTWAIT);
            if(n>0){in_at+=n;in_len-=n;advanced=1;}
            else if(n==0 || (errno!=EAGAIN && errno!=EWOULDBLOCK))break;
        }
        if(eof && !in_len)break;
        if(advanced)progress=xTaskGetTickCount();
        else if((TickType_t)(xTaskGetTickCount()-progress)>pdMS_TO_TICKS(60000))break;
        vTaskDelay(1);
    }
done:
    ESP_LOGI("tls-offload","handshake_result=%d detail=%d cert_flags=%x",status,detail,cert_flags);
    if(tls)esp_tls_conn_destroy(tls);
    shutdown(fd,SHUT_RDWR);close(fd);xSemaphoreGive(capacity);vTaskDelete(NULL);
}
static void listener(void *unused)
{
    (void)unused;int fd=socket(AF_INET,SOCK_STREAM,0);configASSERT(fd>=0);
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(TLS_OFFLOAD_PORT)};
    inet_pton(AF_INET,"192.168.77.1",&address.sin_addr);
    int reuse=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    configASSERT(!bind(fd,(void *)&address,sizeof(address)) && !listen(fd,2));
    for(;;) {
        struct sockaddr_in peer;socklen_t size=sizeof(peer);int client=accept(fd,(void *)&peer,&size);
        if(client<0)continue;
        if(peer.sin_addr.s_addr!=inet_addr("192.168.77.2") || !usb_link || xSemaphoreTake(capacity,0)!=pdTRUE){close(client);continue;}
        if(xTaskCreate(serve,"psp_tls",8192,(void *)(intptr_t)client,4,NULL)!=pdPASS){close(client);xSemaphoreGive(capacity);}
    }
}
void tls_offload_start(void)
{
    capacity=xSemaphoreCreateCounting(TLS_OFFLOAD_SLOTS,TLS_OFFLOAD_SLOTS);configASSERT(capacity);
    configASSERT(xTaskCreate(listener,"psp_tls_listen",4096,NULL,3,NULL)==pdPASS);
}
