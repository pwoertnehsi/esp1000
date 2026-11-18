/* Passive, bounded TLS diagnostics. No ciphertext decryption or payload logging. */
#ifndef PSP_SSL_TRACE_H
#define PSP_SSL_TRACE_H
#include <stdint.h>
#include <stddef.h>
#define SSL_TRACE_FLOWS 4
#define SSL_TRACE_PREFIX 1024
#define SSL_TRACE_EVENTS 48
/* a/b/c interpretation is documented in docs/SSL_CAPTURE.md. */
enum { SSL_T_OPEN=1,SSL_T_RECORD,SSL_T_CLIENT,SSL_T_SERVER,SSL_T_CIPHER,
    SSL_T_VERSIONS,SSL_T_HANDSHAKE,SSL_T_ALERT,SSL_T_CCS,SSL_T_ENCRYPTED,
    SSL_T_BAD,SSL_T_GAP,SSL_T_FIN,SSL_T_RESET,SSL_T_LIMIT,SSL_T_END,SSL_T_SSL2 };
typedef struct {
    uint32_t id,peer,a,b,c;unsigned local_port,remote_port,direction,event;
} ssl_trace_event_t;
typedef void (*ssl_trace_emit_t)(void *,const ssl_trace_event_t *);
typedef struct {
    uint32_t next,bytes;unsigned initialized,disabled,encrypted,fin;
    unsigned header_used,record_left,record_type,record_version,record_size,record_used;
    uint8_t header[5],small[2],handshake[4],prefix[SSL_TRACE_PREFIX];
    unsigned handshake_used,body_left,body_size,body_used,body_type;
} ssl_trace_stream_t;
typedef struct {
    unsigned active,events;uint32_t id,peer;
    unsigned local_port,remote_port;
    ssl_trace_stream_t stream[2];
} ssl_trace_flow_t;
typedef struct {ssl_trace_flow_t flow[SSL_TRACE_FLOWS];uint32_t serial;unsigned evictions;} ssl_trace_t;
void ssl_trace_feed(ssl_trace_t *,int outgoing,const uint8_t *ethernet,size_t length,ssl_trace_emit_t,void *);
void ssl_trace_finish(ssl_trace_t *,ssl_trace_emit_t,void *);
const char *ssl_trace_name(unsigned event);
#endif
