#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "native_profile.h"
#include "network_page.h"
#include "host_wakeup.h"
typedef struct {native_wifi_profile_t profile;unsigned request,epoch;} command_t;
typedef struct {unsigned request,epoch;int result;} reply_t;
static QueueHandle_t commands,replies;
static int active;
static command_t pending;
static void worker(void *unused)
{
    (void)unused;command_t *command;
    for(;;) {
        if(xQueueReceive(commands,&command,portMAX_DELAY)!=pdTRUE)continue;
        reply_t reply={command->request,command->epoch,network_page_apply_profile(&command->profile)};
        native_profile_clear(command,sizeof(*command));
        configASSERT(xQueueSend(replies,&reply,portMAX_DELAY)==pdTRUE);
        host_wakeup_signal(false);
    }
}
void native_profile_start(void)
{
    commands=xQueueCreate(1,sizeof(command_t *));replies=xQueueCreate(1,sizeof(reply_t));
    configASSERT(commands && replies);
    configASSERT(xTaskCreate(worker,"native_profile",4096,NULL,3,NULL)==pdPASS);
}
int native_profile_busy(void){return active;}
int native_profile_submit(const native_wifi_profile_t *profile,unsigned request,unsigned epoch)
{
    if(active || !request || !native_profile_valid(profile))return 0;
    pending=(command_t){.profile=*profile,.request=request,.epoch=epoch};
    command_t *command=&pending;
    int ok=xQueueSend(commands,&command,0)==pdTRUE;
    if(!ok)native_profile_clear(&pending,sizeof(pending));
    if(ok)active=1;
    return ok;
}
int native_profile_take(link_frame_t *frame,unsigned *epoch)
{
    reply_t reply;
    if(xQueueReceive(replies,&reply,0)!=pdTRUE)return 0;
    active=0;*epoch=reply.epoch;
    link_init(frame,LINK_WIFI_PROFILE_RESULT,0,reply.request,NULL);
    frame->length=8;frame->payload[0]=1;link_put_u32(frame->payload+4,reply.result?1:0);return 1;
}
