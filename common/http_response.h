#ifndef PSP_HTTP_RESPONSE_H
#define PSP_HTTP_RESPONSE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define NATIVE_HTTP_HEADER_MAX 4096u
#define NATIVE_HTTP_BODY_MAX (128u*1024u)
static inline unsigned http_lower(unsigned c) { return c>='A' && c<='Z'?c+32:c; }
static inline int http_name(const uint8_t *p,size_t n,const char *name)
{
    size_t m=strlen(name);
    if(n!=m)return 0;
    for(size_t i=0;i<n;++i) if(http_lower(p[i])!=(unsigned char)name[i]) return 0;
    return 1;
}
/* Complete response only. Accept a bounded HTTP/1.0 or /1.1 200 response;
 * HTTP/1.0 request asks for close framing. Reject unsupported transfer coding,
 * duplicate lengths and truncated bodies instead of publishing partial HTML. */
static inline int http_response_body(const uint8_t *p,size_t n,size_t *offset,size_t *length)
{
    if(n<16 || (memcmp(p,"HTTP/1.0 200 ",13) && memcmp(p,"HTTP/1.1 200 ",13)))return 0;
    size_t pos=0; while(pos+1<n && !(p[pos]=='\r'&&p[pos+1]=='\n')) ++pos;
    if(pos+1>=n || pos>NATIVE_HTTP_HEADER_MAX)return 0;
    pos+=2;size_t advertised=0;int has_length=0;
    while(pos+1<n && pos<NATIVE_HTTP_HEADER_MAX) {
        if(p[pos]=='\r' && p[pos+1]=='\n') {
            if(pos+2>NATIVE_HTTP_HEADER_MAX)return 0;
            pos+=2;size_t body=n-pos;
            if(!body || body>NATIVE_HTTP_BODY_MAX || (has_length && body!=advertised))return 0;
            *offset=pos;*length=body;return 1;
        }
        size_t end=pos;while(end+1<n && end<NATIVE_HTTP_HEADER_MAX && !(p[end]=='\r'&&p[end+1]=='\n'))++end;
        if(end+1>=n || end>=NATIVE_HTTP_HEADER_MAX)return 0;
        size_t colon=pos;while(colon<end && p[colon]!=':')++colon;
        if(colon==end || colon==pos)return 0;
        if(http_name(p+pos,colon-pos,"transfer-encoding"))return 0;
        if(http_name(p+pos,colon-pos,"content-encoding")) {
            size_t start=colon+1,stop=end;
            while(start<stop && (p[start]==' '||p[start]=='\t'))++start;
            while(stop>start && (p[stop-1]==' '||p[stop-1]=='\t'))--stop;
            if(!http_name(p+start,stop-start,"identity"))return 0;
        }
        if(http_name(p+pos,colon-pos,"content-length")) {
            if(has_length)return 0;
            has_length=1;size_t i=colon+1;while(i<end && (p[i]==' '||p[i]=='\t'))++i;
            if(i==end)return 0;
            size_t digits=0;
            for(;i<end && p[i]>='0'&&p[i]<='9';++i) {
                advertised=advertised*10+(p[i]-'0');++digits;
                if(advertised>NATIVE_HTTP_BODY_MAX)return 0;
            }
            while(i<end && (p[i]==' '||p[i]=='\t'))++i;
            if(!digits || i!=end)return 0;
        }
        pos=end+2;
    }
    return 0;
}
#endif
