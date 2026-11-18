#ifndef PSP_NATIVE_WIFI_SCAN_H
#define PSP_NATIVE_WIFI_SCAN_H
#include "link_protocol.h"
#define NATIVE_SCAN_LIMIT 32u
#define NATIVE_SCAN_VERSION 1u
#define NATIVE_SCAN_REQUEST LINK_WIFI_SCAN_REQUEST
#define NATIVE_SCAN_RESULT LINK_WIFI_SCAN_RESULT
#define NATIVE_SCAN_ROW 1u
#define NATIVE_SCAN_DONE 2u
#define NATIVE_SCAN_OK 0u
#define NATIVE_SCAN_NOT_READY 1u
#define NATIVE_SCAN_BUSY 2u
#define NATIVE_SCAN_FAILED 3u
#define NATIVE_SCAN_BAD_WIRE 4u
#define NATIVE_SCAN_TIMEOUT 5u
/* Explicit wire values, independent of ESP-IDF enum numbering. */
enum {NATIVE_AUTH_OPEN,NATIVE_AUTH_WEP,NATIVE_AUTH_WPA,NATIVE_AUTH_WPA2,
      NATIVE_AUTH_WPA_MIXED,NATIVE_AUTH_WPA3,NATIVE_AUTH_WPA3_MIXED,
      NATIVE_AUTH_ENTERPRISE,NATIVE_AUTH_UNKNOWN=255};
enum {NATIVE_CIPHER_UNKNOWN,NATIVE_CIPHER_NONE,NATIVE_CIPHER_WEP40,
      NATIVE_CIPHER_WEP104,NATIVE_CIPHER_TKIP,NATIVE_CIPHER_AES};
typedef struct {
    uint8_t bssid[6],ssid[32],ssid_length,channel,auth,cipher;
    int8_t rssi;
} native_scan_row_t;
static inline int native_scan_request_valid(const link_frame_t *frame)
{
    return link_valid(frame,sizeof(*frame)) && frame->type==NATIVE_SCAN_REQUEST && frame->seq && frame->length==4 &&
        frame->payload[0]==NATIVE_SCAN_VERSION && frame->payload[1]>0 &&
        frame->payload[1]<=NATIVE_SCAN_LIMIT && frame->payload[2]<=1 && !frame->payload[3];
}
static inline int native_scan_row_valid(const native_scan_row_t *row)
{
    return row->ssid_length<=32 && row->channel>=1 && row->channel<=14 && row->rssi<=0 &&
        (row->auth<=NATIVE_AUTH_ENTERPRISE || row->auth==NATIVE_AUTH_UNKNOWN) && row->cipher<=NATIVE_CIPHER_AES;
}
static inline int native_scan_encode_row(link_frame_t *frame,unsigned sequence,unsigned request,unsigned index,const native_scan_row_t *row)
{
    if(!request || index>=NATIVE_SCAN_LIMIT || !row || !native_scan_row_valid(row))return 0;
    link_init(frame,NATIVE_SCAN_RESULT,sequence,request,NULL);frame->length=48;
    frame->payload[0]=NATIVE_SCAN_ROW;frame->payload[1]=NATIVE_SCAN_VERSION;
    frame->payload[2]=index;frame->payload[3]=row->channel;
    memcpy(frame->payload+4,row->bssid,6);frame->payload[10]=row->ssid_length;
    frame->payload[11]=(uint8_t)row->rssi;memcpy(frame->payload+12,row->ssid,row->ssid_length);
    frame->payload[44]=row->auth;frame->payload[45]=row->cipher;return 1;
}
static inline int native_scan_encode_done(link_frame_t *frame,unsigned sequence,unsigned request,unsigned count,unsigned result)
{
    if(!request || count>NATIVE_SCAN_LIMIT || result>NATIVE_SCAN_FAILED || (result && count))return 0;
    link_init(frame,NATIVE_SCAN_RESULT,sequence,request,NULL);frame->length=8;
    frame->payload[0]=NATIVE_SCAN_DONE;frame->payload[1]=NATIVE_SCAN_VERSION;
    frame->payload[2]=count;frame->payload[3]=result;return 1;
}
/* Results are not exposed until the complete matching transaction is received. */
typedef struct {
    unsigned request,limit,busy,complete,count,error;
    uint32_t received;
    native_scan_row_t rows[NATIVE_SCAN_LIMIT];
} native_scan_session_t;
static inline int native_scan_begin(native_scan_session_t *scan,unsigned request,unsigned limit)
{
    if(scan->busy || !request || !limit || limit>NATIVE_SCAN_LIMIT)return -1;
    memset(scan,0,sizeof(*scan));scan->request=request;scan->limit=limit;scan->busy=1;return 0;
}
static inline void native_scan_abort(native_scan_session_t *scan,unsigned error)
{
    scan->busy=0;scan->complete=0;scan->count=0;scan->error=error;
    memset(scan->rows,0,sizeof(scan->rows));scan->received=0;
}
static inline int native_scan_consume(native_scan_session_t *scan,const link_frame_t *frame)
{
    if(!scan->busy || frame->type!=NATIVE_SCAN_RESULT || frame->reply_to!=scan->request)return 0;
    if(memcmp(frame->magic,"PSPW",4) || frame->version!=1 || frame->length>48 ||
        frame->length<4 || frame->payload[1]!=NATIVE_SCAN_VERSION)goto invalid;
    if(frame->payload[0]==NATIVE_SCAN_ROW) {
        unsigned index=frame->payload[2];native_scan_row_t row;memset(&row,0,sizeof(row));
        if(frame->length!=48 || index>=scan->limit || frame->payload[10]>32 || frame->payload[46] || frame->payload[47])goto invalid;
        row.channel=frame->payload[3];memcpy(row.bssid,frame->payload+4,6);
        row.ssid_length=frame->payload[10];row.rssi=(int8_t)frame->payload[11];
        memcpy(row.ssid,frame->payload+12,row.ssid_length);row.auth=frame->payload[44];row.cipher=frame->payload[45];
        if(!native_scan_row_valid(&row))goto invalid;
        for(unsigned i=row.ssid_length;i<32;++i)if(frame->payload[12+i])goto invalid;
        uint32_t bit=(uint32_t)1<<index;
        if((scan->received&bit) && memcmp(&scan->rows[index],&row,sizeof(row)))goto invalid;
        scan->rows[index]=row;scan->received|=bit;return 1;
    }
    if(frame->payload[0]==NATIVE_SCAN_DONE) {
        unsigned count=frame->payload[2],error=frame->payload[3];
        if(frame->length!=8 || count>scan->limit || error>NATIVE_SCAN_FAILED || link_get_u32(frame->payload+4))goto invalid;
        if(error){if(count)goto invalid;native_scan_abort(scan,error);return -1;}
        uint32_t required=count==32?UINT32_MAX:((uint32_t)1<<count)-1;
        if(scan->received!=required)goto invalid;
        scan->busy=0;scan->complete=1;scan->count=count;return 2;
    }
invalid:
    native_scan_abort(scan,NATIVE_SCAN_BAD_WIRE);return -1;
}
#endif
