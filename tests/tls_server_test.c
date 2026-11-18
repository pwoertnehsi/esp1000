#include <stdio.h>
#include "tls_server_shim.h"
#include "../esp_link/main/tls_offload.c"
static jmp_buf finished;
static unsigned char input[512],output[512],upstream_data[512];
static unsigned input_size,input_at,output_at,upstream_at,reply_at,ticks,released,closed,verified,write_wait,read_wait;
static int mode;
static const char request[]="GET / HTTP/1.1\r\nHost: en.touhouwiki.net\r\n\r\n";
static const char reply[]="HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
static esp_tls_t context;
static TickType_t xTaskGetTickCount(void){return ticks;}
static void vTaskDelay(unsigned n){ticks+=n;assert(ticks<1000);}
static void vTaskDelete(void *p){assert(!p);longjmp(finished,1);}
static int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle)
{(void)fn;(void)name;(void)stack;(void)arg;(void)priority;(void)handle;return pdPASS;}
static void *xSemaphoreCreateCounting(unsigned a,unsigned b){assert(a==2 && b==2);return &context;}
static int xSemaphoreTake(void *a,unsigned b){(void)a;(void)b;return 1;}
static void xSemaphoreGive(void *a){assert(a==capacity);++released;}
static int mock_recv(int fd,void *p,size_t n,int flags)
{
    assert(fd==10);
    if(input_at<input_size){if(n>2)n=2;if(n>input_size-input_at)n=input_size-input_at;memcpy(p,input+input_at,n);input_at+=n;return n;}
    if(!flags)return 0;
    if(upstream_at<sizeof(request)-1){if(n>3)n=3;if(n>sizeof(request)-1-upstream_at)n=sizeof(request)-1-upstream_at;memcpy(p,request+upstream_at,n);/* advance at TLS write, not receive */return n;}
    errno=EAGAIN;return -1;
}
static int mock_send(int fd,const void *p,size_t n,int flags)
{(void)flags;assert(fd==10);if(n>2)n=2;assert(output_at+n<=sizeof(output));memcpy(output+output_at,p,n);output_at+=n;return n;}
static int mock_close(int fd){assert(fd==10);++closed;return 0;}
static int mock_shutdown(int fd,int how){assert(fd==10 && how==SHUT_RDWR);return 0;}
static int mock_fcntl(int fd,int command,...){assert(fd==10 || fd==20);(void)command;return 0;}
static int mock_setsockopt(int fd,int level,int option,const void *value,socklen_t n){(void)level;(void)option;(void)value;(void)n;assert(fd==10);return 0;}
static time_t mock_time(time_t *p){time_t value=mode==2?0:1800000000;if(p)*p=value;return value;}
static int esp_netif_sntp_sync_wait(unsigned n){assert(n==12000);return -1;}
static int esp_crt_bundle_attach(void *ctx){(void)ctx;return 0;}
static esp_tls_t *esp_tls_init(void){return &context;}
static void esp_tls_conn_destroy(esp_tls_t *ctx){assert(ctx==&context);}
static int esp_tls_conn_new_sync(const char *host,int n,int port,const esp_tls_cfg_t *cfg,esp_tls_t *ctx)
{assert(ctx==&context && port==443 && n==17 && !strcmp(host,"en.touhouwiki.net") && cfg->crt_bundle_attach==esp_crt_bundle_attach && !cfg->skip_common_name && cfg->alpn_protos && !strcmp(cfg->alpn_protos[0],"http/1.1"));++verified;return mode==1?-1:1;}
static int esp_tls_get_error_handle(esp_tls_t *ctx,esp_tls_error_handle_t *out){assert(ctx==&context);*out=ctx;return 0;}
static int esp_tls_get_and_clear_last_error(esp_tls_error_handle_t h,int *detail,int *flags){assert(h==&context);*detail=-0x2700;*flags=1;return 0;}
static int esp_tls_get_conn_sockfd(esp_tls_t *ctx,int *fd){assert(ctx==&context);*fd=20;return 0;}
static int esp_tls_conn_write(esp_tls_t *ctx,const void *p,size_t n)
{assert(ctx==&context);if(!write_wait++){return ESP_TLS_ERR_SSL_WANT_READ;}if(n>2)n=2;assert(upstream_at+n<=sizeof(upstream_data));memcpy(upstream_data+upstream_at,p,n);upstream_at+=n;return n;}
static int esp_tls_conn_read(esp_tls_t *ctx,void *p,size_t n)
{assert(ctx==&context);if(upstream_at<sizeof(request)-1 || !read_wait++)return ESP_TLS_ERR_SSL_WANT_WRITE;if(reply_at==sizeof(reply)-1)return 0;if(n>3)n=3;if(n>sizeof(reply)-1-reply_at)n=sizeof(reply)-1-reply_at;memcpy(p,reply+reply_at,n);reply_at+=n;return n;}
static void run(int scenario)
{
    mode=scenario;input_size=input_at=output_at=upstream_at=reply_at=ticks=released=closed=verified=write_wait=read_wait=0;
    const char host[]="en.touhouwiki.net";tls_offload_request(input,sizeof(host)-1,443);memcpy(input+8,host,sizeof(host)-1);input_size=8+sizeof(host)-1;
    if(mode==3)input[0]^=1;
    usb_link=1;
    if(!setjmp(finished))serve((void *)10);
    assert(closed==1 && released==1);
    if(mode==3){assert(!verified && !output_at);return;}
    int32_t status;assert(tls_offload_reply_status(output,&status));
    if(mode){assert(status<0 && output_at==TLS_OFFLOAD_REPLY && !upstream_at);return;}
    assert(!status && verified==1 && output_at==TLS_OFFLOAD_REPLY+sizeof(reply)-1);
    assert(!memcmp(output+TLS_OFFLOAD_REPLY,reply,sizeof(reply)-1) && !memcmp(upstream_data,request,sizeof(request)-1));
}
int main(void)
{
    tls_offload_start();run(0);run(1);run(2);run(3);
    puts("PASS: actual ESP TLS service certificate/SNI configuration, fail-closed ACK, unset clock, partial duplex I/O and cleanup");
}
