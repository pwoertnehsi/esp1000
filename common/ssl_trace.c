#include "ssl_trace.h"
#include "packet_trace.h"
static void event(ssl_trace_flow_t *f,unsigned dir,unsigned kind,unsigned a,unsigned b,unsigned c,ssl_trace_emit_t emit,void *ctx)
{
    int terminal=kind==SSL_T_END || kind==SSL_T_FIN || kind==SSL_T_RESET;
    if(!terminal && f->events>=SSL_TRACE_EVENTS)return;
    if(!terminal && ++f->events==SSL_TRACE_EVENTS)kind=SSL_T_LIMIT;
    ssl_trace_event_t e={f->id,f->peer,a,b,c,f->local_port,f->remote_port,dir,kind};
    emit(ctx,&e);
}
static void bad(ssl_trace_flow_t *f,unsigned dir,unsigned reason,ssl_trace_emit_t emit,void *ctx)
{f->stream[dir].disabled=1;event(f,dir,SSL_T_BAD,reason,0,0,emit,ctx);}
static void hello(ssl_trace_flow_t *f,unsigned dir,ssl_trace_emit_t emit,void *ctx)
{
    ssl_trace_stream_t *s=&f->stream[dir];const uint8_t *p=s->prefix;unsigned n=s->body_size;
    if(n>SSL_TRACE_PREFIX){event(f,dir,SSL_T_BAD,5,n,SSL_TRACE_PREFIX,emit,ctx);return;}
    if(n<35){bad(f,dir,6,emit,ctx);return;}
    unsigned version=trace_be16(p),at=35+p[34],count=0,ciphers=0,selected=0;
    if(p[34]>32 || at>n){bad(f,dir,6,emit,ctx);return;}
    if(s->body_type==1) {
        if(n-at<2){bad(f,dir,6,emit,ctx);return;}
        unsigned bytes=trace_be16(p+at);at+=2;ciphers=at;
        if(!bytes || (bytes&1) || bytes>n-at){bad(f,dir,6,emit,ctx);return;}
        count=bytes/2;at+=bytes;
        if(at>=n || !p[at] || p[at]>n-at-1){bad(f,dir,6,emit,ctx);return;}
        at+=1+p[at];
    } else {
        if(n-at<3){bad(f,dir,6,emit,ctx);return;}
        selected=trace_be16(p+at);at+=3;
    }
    unsigned flags=0,highest=0,versions=0;
    if(at<n) {
        if(n-at<2 || trace_be16(p+at)!=n-at-2){bad(f,dir,7,emit,ctx);return;}
        at+=2;
        while(at<n) {
            if(n-at<4){bad(f,dir,7,emit,ctx);return;}
            unsigned type=trace_be16(p+at),size=trace_be16(p+at+2);at+=4;
            if(size>n-at){bad(f,dir,7,emit,ctx);return;}
            if(type==0)flags|=1; /* Presence only; never store/log the hostname. */
            if(type==43) {
                flags|=2;
                if(s->body_type==1) {
                    if(!size || p[at]!=size-1 || ((size-1)&1)){bad(f,dir,7,emit,ctx);return;}
                    versions=(size-1)/2;
                    for(unsigned i=1;i<size;i+=2){unsigned v=trace_be16(p+at+i);if(v>highest)highest=v;}
                } else {
                    if(size!=2){bad(f,dir,7,emit,ctx);return;}highest=trace_be16(p+at);versions=1;
                }
            }
            at+=size;
        }
    }
    event(f,dir,s->body_type==1?SSL_T_CLIENT:SSL_T_SERVER,version,s->body_type==1?count:selected,flags,emit,ctx);
    if(ciphers)for(unsigned i=0;i<count && i<8;++i)event(f,dir,SSL_T_CIPHER,trace_be16(p+ciphers+2*i),i,count,emit,ctx);
    if(versions)event(f,dir,SSL_T_VERSIONS,highest,versions,0,emit,ctx);
}
static void handshake_byte(ssl_trace_flow_t *f,unsigned dir,uint8_t byte,ssl_trace_emit_t emit,void *ctx)
{
    ssl_trace_stream_t *s=&f->stream[dir];
    if(s->handshake_used<4) {
        s->handshake[s->handshake_used++]=byte;
        if(s->handshake_used<4)return;
        s->body_type=s->handshake[0];s->body_size=(unsigned)s->handshake[1]<<16|(unsigned)s->handshake[2]<<8|s->handshake[3];
        s->body_left=s->body_size;s->body_used=0;
        if(s->body_size>1048576){bad(f,dir,8,emit,ctx);return;}
        event(f,dir,SSL_T_HANDSHAKE,s->body_type,s->body_size,0,emit,ctx);
        if(s->body_left)return;
    } else {
        if((s->body_type==1 || s->body_type==2) && s->body_used<SSL_TRACE_PREFIX)s->prefix[s->body_used]=byte;
        ++s->body_used;--s->body_left;
        if(s->body_left)return;
    }
    if(s->body_type==1 || s->body_type==2)hello(f,dir,emit,ctx);
    memset(s->prefix,0,sizeof(s->prefix));s->handshake_used=0;
}
static void ssl2_hello(ssl_trace_flow_t *f,unsigned dir,ssl_trace_emit_t emit,void *ctx)
{
    ssl_trace_stream_t *s=&f->stream[dir];uint8_t *p=s->prefix;unsigned n=s->record_size;
    if(n>SSL_TRACE_PREFIX || n<9 || p[0]!=1){bad(f,dir,9,emit,ctx);return;}
    unsigned bytes=trace_be16(p+3),session=trace_be16(p+5),challenge=trace_be16(p+7);
    if(bytes%3 || !bytes || bytes+session+challenge!=n-9){bad(f,dir,9,emit,ctx);return;}
    event(f,dir,SSL_T_SSL2,trace_be16(p+1),bytes/3,0,emit,ctx);
    for(unsigned i=0;i<bytes/3 && i<8;++i)event(f,dir,SSL_T_CIPHER,(unsigned)p[9+i*3]<<16|trace_be16(p+10+i*3),i,bytes/3,emit,ctx);
    memset(s->prefix,0,sizeof(s->prefix));
}
static void bytes(ssl_trace_flow_t *f,unsigned dir,const uint8_t *p,unsigned n,ssl_trace_emit_t emit,void *ctx)
{
    ssl_trace_stream_t *s=&f->stream[dir];
    for(unsigned i=0;i<n && !s->disabled;++i) {
        uint8_t byte=p[i];
        if(!s->record_left) {
            s->header[s->header_used++]=byte;
            unsigned need=(s->header[0]&128)?2:5;
            if(s->header_used<need)continue;
            if(need==2) {
                if(dir!=0 || s->encrypted){bad(f,dir,1,emit,ctx);break;}
                s->record_type=128;s->record_version=0;s->record_size=((s->header[0]&127)<<8)|s->header[1];
            } else {
                s->record_type=s->header[0];s->record_version=trace_be16(s->header+1);s->record_size=trace_be16(s->header+3);
                if(s->header[1]!=3 || s->header[2]>4 || s->record_type<20 || s->record_type>24){bad(f,dir,1,emit,ctx);break;}
            }
            if(s->record_size>18432 || (s->record_type==128 && !s->record_size)){bad(f,dir,2,emit,ctx);break;}
            s->header_used=0;s->record_left=s->record_size;s->record_used=0;
            /* Ciphertext record framing is visible; its contents are not decoded. */
            event(f,dir,s->encrypted || s->record_type==23?SSL_T_ENCRYPTED:SSL_T_RECORD,s->record_type,s->record_version,s->record_size,emit,ctx);
            continue;
        }
        if(!s->encrypted) {
            if(s->record_type==22)handshake_byte(f,dir,byte,emit,ctx);
            else if(s->record_type==128 && s->record_used<SSL_TRACE_PREFIX)s->prefix[s->record_used]=byte;
            else if((s->record_type==20 || s->record_type==21) && s->record_used<2)s->small[s->record_used]=byte;
        }
        ++s->record_used;--s->record_left;
        if(!s->record_left && !s->encrypted) {
            if(s->record_type==21) {
                if(s->record_size==2)event(f,dir,SSL_T_ALERT,s->small[0],s->small[1],s->record_version,emit,ctx);
                else bad(f,dir,3,emit,ctx);
            } else if(s->record_type==20) {
                if(s->record_size==1 && s->small[0]==1){s->encrypted=1;event(f,dir,SSL_T_CCS,0,0,0,emit,ctx);}
                else bad(f,dir,4,emit,ctx);
            } else if(s->record_type==128)ssl2_hello(f,dir,emit,ctx);
        }
    }
}
static void end(ssl_trace_flow_t *f,ssl_trace_emit_t emit,void *ctx)
{
    event(f,0,SSL_T_END,f->stream[0].bytes,f->stream[1].bytes,f->stream[0].disabled|f->stream[1].disabled<<1,emit,ctx);
    memset(f,0,sizeof(*f));
}
void ssl_trace_feed(ssl_trace_t *t,int outgoing,const uint8_t *frame,size_t length,ssl_trace_emit_t emit,void *ctx)
{
    /* Non-HTTPS traffic, including Ad Hoc packets, takes this cheap path. */
    if(length<54 || frame[12]!=8 || frame[13] || frame[14]>>4!=4 || frame[23]!=6)return;
    unsigned ihl=(frame[14]&15)*4;
    if(ihl<20 || length<14+ihl+20)return;
    const uint8_t *tcp=frame+14+ihl;
    if(trace_be16(tcp+(outgoing?2:0))!=443)return;
    packet_trace_t p;
    if(!packet_trace_parse(&p,frame,length) || p.protocol!=6 || !p.ip_ok || p.tcp_ok!=1)return;
    unsigned dir=outgoing?0:1,local=outgoing?p.sport:p.dport,remote=outgoing?p.dport:p.sport;
    if(remote!=443)return;
    uint32_t peer=trace_be32(outgoing?p.destination:p.source);ssl_trace_flow_t *f=NULL;
    for(unsigned i=0;i<SSL_TRACE_FLOWS;++i)if(t->flow[i].active && t->flow[i].peer==peer && t->flow[i].local_port==local){f=&t->flow[i];break;}
    if(!f) {
        if(dir || !(p.flags&2))return; /* Never guess a midstream record boundary. */
        for(unsigned i=0;i<SSL_TRACE_FLOWS;++i)if(!t->flow[i].active){f=&t->flow[i];break;}
        if(!f){f=&t->flow[0];for(unsigned i=1;i<SSL_TRACE_FLOWS;++i)if(t->flow[i].id<f->id)f=&t->flow[i];end(f,emit,ctx);++t->evictions;}
        f->active=1;f->id=++t->serial;f->peer=peer;f->local_port=local;f->remote_port=remote;
        event(f,dir,SSL_T_OPEN,0,0,0,emit,ctx);
    }
    ssl_trace_stream_t *s=&f->stream[dir];uint32_t seq=p.seq+!!(p.flags&2);
    if(p.flags&2) {if(!s->initialized){s->initialized=1;s->next=seq;}}
    if(p.payload) {
        if(!s->initialized){s->initialized=1;s->disabled=1;event(f,dir,SSL_T_GAP,0,seq,p.payload,emit,ctx);}
        int32_t delta=(int32_t)(seq-s->next);
        if(delta>0 && !s->disabled){event(f,dir,SSL_T_GAP,s->next,seq,p.payload,emit,ctx);s->disabled=1;}
        unsigned skip=delta<0?s->next-seq:0;
        if(skip<p.payload) {
            unsigned ihl=(frame[14]&15)*4;const uint8_t *tcp=frame+14+ihl;
            const uint8_t *data=tcp+(tcp[12]>>4)*4;
            bytes(f,dir,data+skip,p.payload-skip,emit,ctx);
            s->bytes+=p.payload-skip;s->next=seq+p.payload;
        }
    }
    if((p.flags&1) && !s->fin){s->fin=1;event(f,dir,SSL_T_FIN,s->next,0,0,emit,ctx);}
    if(p.flags&4){event(f,dir,SSL_T_RESET,p.seq,p.ack,0,emit,ctx);end(f,emit,ctx);}
    else if(f->stream[0].fin && f->stream[1].fin)end(f,emit,ctx);
}
void ssl_trace_finish(ssl_trace_t *t,ssl_trace_emit_t emit,void *ctx)
{for(unsigned i=0;i<SSL_TRACE_FLOWS;++i)if(t->flow[i].active)end(&t->flow[i],emit,ctx);}
const char *ssl_trace_name(unsigned kind)
{
    static const char *names[]={"UNKNOWN","OPEN","RECORD","CLIENT_HELLO","SERVER_HELLO","CIPHER","VERSIONS","HANDSHAKE","ALERT","CHANGE_CIPHER","ENCRYPTED","DECODE_STOP","TCP_GAP","FIN","RESET","LIMIT","END","SSL2_CLIENT_HELLO"};
    return kind<sizeof(names)/sizeof(names[0])?names[kind]:names[0];
}
