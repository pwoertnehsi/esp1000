#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host_wakeup.h"

static TaskHandle_t owner;

void host_wakeup_bind(void)
{
    owner = xTaskGetCurrentTaskHandle();
}

void host_wakeup_signal(bool in_isr)
{
    if (!owner) return;
    if (in_isr) {
        BaseType_t woken = 0;
        vTaskNotifyGiveFromISR(owner, &woken);
        if (woken) portYIELD_FROM_ISR();
    } else xTaskNotifyGive(owner);
}

void host_wakeup_wait(void)
{
    /* Notifications survive completion-before-wait and coalesce bursts.
     * One tick is a fallback for timers, not a sleep after every fragment. */
    ulTaskNotifyTake(pdTRUE, 1);
}
