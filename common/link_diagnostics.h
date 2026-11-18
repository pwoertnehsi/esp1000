#ifndef PSP_LINK_DIAGNOSTICS_H
#define PSP_LINK_DIAGNOSTICS_H
#include "link_protocol.h"
#define LINK_CHECK_SIZE 1u
#define LINK_CHECK_MAGIC 2u
#define LINK_CHECK_VERSION 4u
#define LINK_CHECK_TYPE 8u
#define LINK_CHECK_LENGTH 16u
/* Reasons only: never expose payload, raw header bytes or credentials. Do not
 * inspect the frame at all when the transfer is short or its buffer absent. */
static inline unsigned link_frame_checks(const link_frame_t *f,unsigned size)
{
    if(size!=sizeof(*f) || !f)return LINK_CHECK_SIZE;
    unsigned result=0;
    if(memcmp(f->magic,"PSPW",4))result|=LINK_CHECK_MAGIC;
    if(f->version!=1)result|=LINK_CHECK_VERSION;
    if(f->type<LINK_PING || f->type>LINK_LAST_TYPE)result|=LINK_CHECK_TYPE;
    if(f->length>LINK_PAYLOAD_SIZE)result|=LINK_CHECK_LENGTH;
    return result;
}
#endif
