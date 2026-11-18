#include <assert.h>
#include <stdio.h>
#include "../common/link_diagnostics.h"
int main(void)
{
    link_frame_t f;
    for(unsigned type=LINK_PING;type<=LINK_LAST_TYPE;++type) {
        link_init(&f,type,1,0,NULL);
        assert(!link_frame_checks(&f,sizeof(f)) && link_valid(&f,sizeof(f)));
    }
    /* Short data is never inspected, even when the pointer is absent. */
    assert(link_frame_checks(NULL,0)==LINK_CHECK_SIZE);
    assert(link_frame_checks(NULL,64)==LINK_CHECK_SIZE);
    unsigned char one=0;
    assert(link_frame_checks((const void *)&one,1)==LINK_CHECK_SIZE);
    link_init(&f,LINK_PACKET,1,0,NULL);
    memset(f.payload,0xff,sizeof(f.payload));
    assert(!link_frame_checks(&f,sizeof(f))); /* Payload never affects header reasons. */
    f.magic[0]^=1;assert(link_frame_checks(&f,64)==LINK_CHECK_MAGIC);f.magic[0]^=1;
    f.version=2;assert(link_frame_checks(&f,64)==LINK_CHECK_VERSION);f.version=1;
    f.type=0;assert(link_frame_checks(&f,64)==LINK_CHECK_TYPE);f.type=LINK_LAST_TYPE+1;
    assert(link_frame_checks(&f,64)==LINK_CHECK_TYPE);f.type=LINK_PACKET;
    f.length=49;assert(link_frame_checks(&f,64)==LINK_CHECK_LENGTH);
    f.magic[0]=0;f.version=0;f.type=0;
    assert(link_frame_checks(&f,64)==(LINK_CHECK_MAGIC|LINK_CHECK_VERSION|LINK_CHECK_TYPE|LINK_CHECK_LENGTH));
    assert(!link_valid(&f,64));
    puts("PASS: bounded frame reason masks, short-buffer exclusion and payload-independent diagnostics");
}
