#ifndef TLS_SERVER_SHIM_H
#define TLS_SERVER_SHIM_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <setjmp.h>
#define pdMS_TO_TICKS(n) (n)
#define pdTRUE 1
#define pdPASS 1
#define ESP_OK 0
#define ESP_TLS_ERR_SSL_WANT_READ -0x6900
#define ESP_TLS_ERR_SSL_WANT_WRITE -0x6880
#define configASSERT assert
typedef unsigned TickType_t;typedef void *SemaphoreHandle_t;
typedef void *esp_tls_error_handle_t;
typedef struct {int unused;} esp_tls_t;
typedef struct {int (*crt_bundle_attach)(void *);int timeout_ms;const char **alpn_protos;bool skip_common_name;} esp_tls_cfg_t;
static void mock_log(const char *tag,const char *fmt,...){(void)tag;(void)fmt;}
#define ESP_LOGI mock_log
static int mock_recv(int,void *,size_t,int);static int mock_send(int,const void *,size_t,int);
static int mock_close(int);static int mock_shutdown(int,int);static int mock_fcntl(int,int,...);
static int mock_setsockopt(int,int,int,const void *,socklen_t);
static time_t mock_time(time_t *);
#define recv mock_recv
#define send mock_send
#define close mock_close
#define shutdown mock_shutdown
#define fcntl mock_fcntl
#define setsockopt mock_setsockopt
#define time mock_time
static TickType_t xTaskGetTickCount(void);static void vTaskDelay(unsigned);static void vTaskDelete(void *);
static int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,void *);
static void *xSemaphoreCreateCounting(unsigned,unsigned);static int xSemaphoreTake(void *,unsigned);static void xSemaphoreGive(void *);
static int esp_crt_bundle_attach(void *);
static int esp_netif_sntp_sync_wait(unsigned);
static esp_tls_t *esp_tls_init(void);static void esp_tls_conn_destroy(esp_tls_t *);
static int esp_tls_conn_new_sync(const char *,int,int,const esp_tls_cfg_t *,esp_tls_t *);
static int esp_tls_get_error_handle(esp_tls_t *,esp_tls_error_handle_t *);
static int esp_tls_get_and_clear_last_error(esp_tls_error_handle_t,int *,int *);
static int esp_tls_get_conn_sockfd(esp_tls_t *,int *);
static int esp_tls_conn_write(esp_tls_t *,const void *,size_t);static int esp_tls_conn_read(esp_tls_t *,void *,size_t);
#endif
