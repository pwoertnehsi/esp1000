#include <assert.h>
#include <stdio.h>
#include "file_receiver.h"

static uint8_t work[1024], saved[1024];
static unsigned used, saved_size, begins, writes, commits, aborts;
static int fail_write, fail_commit;
static int begin(void *ctx) { (void)ctx; used=0; begins++; return 0; }
static int write_data(void *ctx, const void *p, unsigned n)
{
    (void)ctx; writes++;
    assert(used+n <= sizeof(work));
    memcpy(work+used,p,n); used+=n;
    return fail_write ? -1 : 0;
}
static int commit(void *ctx)
{
    (void)ctx;
    if (fail_commit) return -1;
    memcpy(saved,work,used); saved_size=used; commits++;
    return 0;
}
static void abort_data(void *ctx) { (void)ctx; used=0; aborts++; }
static const link_file_ops_t ops={begin,write_data,commit,abort_data};
static uint32_t seq;

static link_frame_t metadata(unsigned type, uint32_t session, unsigned size, uint32_t crc)
{
    link_frame_t f;
    link_init(&f,type,++seq,session,NULL);
    f.length=8;
    link_put_u32(f.payload,size);
    link_put_u32(f.payload+4,crc);
    return f;
}
static void roundtrip(const uint8_t *data, unsigned size, int corrupt)
{
    link_file_receiver_t receiver={0};
    uint32_t crc=link_crc_update(UINT32_MAX,data,size)^UINT32_MAX;
    link_frame_t request=metadata(LINK_FILE_BEGIN,0,size,crc), reply, duplicate;
    uint32_t session=request.seq;
    unsigned start_begins=begins, start_writes=writes, start_commits=commits;
    link_file_receive(&receiver,&ops,NULL,&request,&reply);
    assert(reply.type==LINK_FILE_ACK && reply.reply_to==request.seq);
    link_file_receive(&receiver,&ops,NULL,&request,&duplicate);
    assert(!memcmp(&reply,&duplicate,sizeof(reply)) && begins==start_begins+1);
    for (unsigned offset=0;offset<size;) {
        unsigned n=size-offset;
        if(n>LINK_FILE_CHUNK)n=LINK_FILE_CHUNK;
        link_init(&request,LINK_FILE_DATA,++seq,session,NULL);
        request.length=4+n;
        link_put_u32(request.payload,offset);
        memcpy(request.payload+4,data+offset,n);
        if(corrupt && offset==0)request.payload[4]^=1;
        assert(link_valid(&request,sizeof(request)));
        link_file_receive(&receiver,&ops,NULL,&request,&reply);
        assert(reply.type==LINK_FILE_ACK && link_get_u32(reply.payload)==offset+n);
        link_file_receive(&receiver,&ops,NULL,&request,&duplicate);
        assert(!memcmp(&reply,&duplicate,sizeof(reply)));
        offset+=n;
    }
    assert(writes==start_writes+(size+43)/44);
    request=metadata(LINK_FILE_END,session,size,crc);
    link_file_receive(&receiver,&ops,NULL,&request,&reply);
    if(corrupt) {
        assert(reply.type==LINK_FILE_ERROR && link_get_u32(reply.payload+4)==FILE_BAD_CHECKSUM);
        assert(commits==start_commits);
    } else {
        assert(reply.type==LINK_FILE_ACK && saved_size==size && !memcmp(saved,data,size));
        link_file_receive(&receiver,&ops,NULL,&request,&duplicate);
        assert(commits==start_commits+1 && !memcmp(&reply,&duplicate,sizeof(reply)));
    }
}
int main(void)
{
    assert((link_crc_update(UINT32_MAX,"123456789",9)^UINT32_MAX)==0xcbf43926u);
    uint8_t data[1000];
    for(unsigned i=0;i<sizeof(data);i++)data[i]=(uint8_t)(i*37);
    roundtrip(data,1,0);
    roundtrip(data,44,0);
    roundtrip(data,sizeof(data),0); /* Binary NUL and non-text bytes. */
    unsigned previous_commits=commits;
    roundtrip(data,97,1);
    assert(commits==previous_commits); /* Corrupt attempt preserves final file. */
    link_file_receiver_t r={0};
    link_frame_t q=metadata(LINK_FILE_BEGIN,0,LINK_FILE_LIMIT+1,0), a;
    link_file_receive(&r,&ops,NULL,&q,&a);
    assert(a.type==LINK_FILE_ERROR && !r.active);
    q=metadata(LINK_FILE_BEGIN,0,10,0);
    link_file_receive(&r,&ops,NULL,&q,&a);
    uint32_t session=q.seq;
    link_init(&q,LINK_FILE_DATA,++seq,session,NULL);
    q.length=5; link_put_u32(q.payload,3);
    link_file_receive(&r,&ops,NULL,&q,&a);
    assert(a.type==LINK_FILE_ERROR && link_get_u32(a.payload+4)==FILE_BAD_OFFSET && !r.active);
    q=metadata(LINK_FILE_BEGIN,0,10,0);
    link_file_receive(&r,&ops,NULL,&q,&a); session=q.seq;
    link_init(&q,LINK_FILE_DATA,++seq,session,NULL);
    q.length=5; link_put_u32(q.payload,0); fail_write=1;
    link_file_receive(&r,&ops,NULL,&q,&a);
    assert(a.type==LINK_FILE_ERROR && !r.active && r.offset==0);
    fail_write=0;
    q=metadata(LINK_FILE_BEGIN,0,10,0);
    link_file_receive(&r,&ops,NULL,&q,&a);
    unsigned before_abort=aborts;
    link_file_abort(&r,&ops,NULL);
    assert(!r.active && !r.have_reply && aborts==before_abort+1);
    fail_commit=1;
    /* A valid one-byte stream fails if publication fails, without overwriting. */
    q=metadata(LINK_FILE_BEGIN,0,1,link_crc_update(UINT32_MAX,data,1)^UINT32_MAX);
    link_file_receive(&r,&ops,NULL,&q,&a); session=q.seq;
    link_init(&q,LINK_FILE_DATA,++seq,session,NULL);
    q.length=5; link_put_u32(q.payload,0); q.payload[4]=data[0];
    link_file_receive(&r,&ops,NULL,&q,&a);
    q=metadata(LINK_FILE_END,session,1,r.expected_crc);
    link_file_receive(&r,&ops,NULL,&q,&a);
    assert(a.type==LINK_FILE_ERROR && commits==previous_commits);
    puts("PASS: binary file assembly, CRC, duplicate retries, bad offsets, IO errors, disconnect abort");
    return 0;
}
