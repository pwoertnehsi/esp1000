/* Diagnostic service bound only to the private USB LAN. It exercises genuine
 * lwIP TCP without Wi-Fi/NAPT and never proxies an internet request. */
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
void local_tcp_start(void);
static void serve(void *unused)
{
    (void)unused;
    int listener=socket(AF_INET,SOCK_STREAM,0);configASSERT(listener>=0);
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(8080)};
    inet_pton(AF_INET,"192.168.77.1",&address.sin_addr);
    int reuse=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    configASSERT(!bind(listener,(void *)&address,sizeof(address)) && !listen(listener,2));
    ESP_LOGI("local-tcp","Local TCP probe listening on USB LAN port8080");
    for(;;) {
        int client=accept(listener,NULL,NULL);if(client<0)continue;
        struct timeval timeout={.tv_sec=5};
        setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
        char request[1024];unsigned used=0;
        while(used+1<sizeof(request)) {
            int n=recv(client,request+used,sizeof(request)-1-used,0);
            if(n<=0)break;
            used+=n;request[used]=0;
            if(strstr(request,"\r\n\r\n"))break;
        }
        if(used>=6 && !memcmp(request,"GET / ",6) && strstr(request,"\r\n\r\n")) {
            static const char response[]="HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><body>PSP native TCP probe</body></html>\n";
            unsigned sent=0;
            while(sent<sizeof(response)-1) {
                int n=send(client,response+sent,sizeof(response)-1-sent,0);
                if(n<=0)break;
                sent+=n;
            }
            ESP_LOGI("local-tcp","Probe request bytes=%u response=%u",used,sent);
        }
        close(client);
    }
}
void local_tcp_start(void) {configASSERT(xTaskCreate(serve,"local_tcp",6144,NULL,4,NULL)==pdPASS);}
