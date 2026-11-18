#ifndef VNET_LOG_QUEUE_H
#define VNET_LOG_QUEUE_H
#include <string.h>
#define VNET_LOG_ROWS 32
#define VNET_LOG_WIDTH 224
/* Call under the PSP interrupt lock; IO is always outside that lock. */
typedef struct {unsigned read,write,dropped;char rows[VNET_LOG_ROWS][VNET_LOG_WIDTH];} vnet_log_queue_t;
static inline int vnet_log_push(vnet_log_queue_t *q,const char *text)
{
    if(q->write-q->read>=VNET_LOG_ROWS){++q->dropped;return 0;}
    unsigned n=0;while(n<VNET_LOG_WIDTH-1 && text[n])++n;
    memcpy(q->rows[q->write%VNET_LOG_ROWS],text,n);
    q->rows[q->write%VNET_LOG_ROWS][n]=0;++q->write;return 1;
}
static inline int vnet_log_pop(vnet_log_queue_t *q,char out[VNET_LOG_WIDTH])
{
    if(q->read==q->write)return 0;
    memcpy(out,q->rows[q->read%VNET_LOG_ROWS],VNET_LOG_WIDTH);++q->read;return 1;
}
#endif
