#include <assert.h>
#include <stdio.h>
#include "../esp_link/main/host_wakeup.c"

static unsigned pending, blocked, yields, task_signals, isr_signals;
static int waiting;
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return (TaskHandle_t)1; }
void xTaskNotifyGive(TaskHandle_t task)
{ assert(task==(TaskHandle_t)1); ++pending; ++task_signals; }
void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *woken)
{ assert(task==(TaskHandle_t)1); ++pending; ++isr_signals; *woken=waiting; }
void test_yield(void) { ++yields; }
unsigned ulTaskNotifyTake(BaseType_t clear, unsigned timeout)
{
    assert(clear==pdTRUE && timeout==1);
    unsigned value=pending;
    if(!value) ++blocked;
    pending=0;
    return value;
}
int main(void)
{
    /* IRQs before owner registration cannot target an invalid task. */
    host_wakeup_signal(true); host_wakeup_signal(false);
    assert(!pending && !isr_signals && !task_signals);
    host_wakeup_bind();
    /* An idle owner still wakes periodically to service enumeration timers,
     * metadata, watchdogs and commands that have no USB completion. */
    host_wakeup_wait(); assert(blocked==1);
    /* Completion between the service pass and wait must remain pending,
     * including a burst of ISR completions and router output. */
    for(unsigned i=0;i<1000;++i) host_wakeup_signal(true);
    host_wakeup_signal(false);
    assert(pending==1001 && !yields);
    host_wakeup_wait(); assert(blocked==1 && !pending);
    /* ISR wakes a sleeping owner immediately rather than waiting one tick. */
    waiting=1; host_wakeup_signal(true); waiting=0;
    assert(yields==1 && pending==1);
    host_wakeup_wait(); assert(blocked==1 && !pending);
    host_wakeup_wait(); assert(blocked==2);
    puts("PASS: early IRQ, idle timeout, completion-before-wait, burst coalescing and ISR wake");
}
