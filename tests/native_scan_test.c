#include <assert.h>
#include <stdio.h>
#include "../common/native_wifi_scan.h"
int main(void)
{
    native_scan_session_t scan={0};link_frame_t frame;
    native_scan_row_t row={.bssid={1,2,3,4,5,6},.ssid={'a','p'},.ssid_length=2,
        .channel=6,.auth=NATIVE_AUTH_WPA2,.cipher=NATIVE_CIPHER_AES,.rssi=-47};
    link_init(&frame,NATIVE_SCAN_REQUEST,7,0,NULL);frame.length=4;
    frame.payload[0]=1;frame.payload[1]=32;assert(native_scan_request_valid(&frame));
    frame.magic[0]=0;assert(!native_scan_request_valid(&frame));
    assert(!native_scan_begin(&scan,7,32));assert(native_scan_begin(&scan,8,32)<0);
    assert(native_scan_encode_row(&frame,9,8,0,&row));assert(!native_scan_consume(&scan,&frame));
    assert(native_scan_encode_row(&frame,9,7,0,&row));assert(native_scan_consume(&scan,&frame)==1);
    assert(!scan.complete && scan.busy);assert(native_scan_consume(&scan,&frame)==1);
    assert(native_scan_encode_done(&frame,10,7,1,0));assert(native_scan_consume(&scan,&frame)==2);
    assert(scan.complete && !scan.busy && scan.count==1 && !memcmp(scan.rows[0].ssid,"ap",2));
    assert(!native_scan_begin(&scan,8,32));
    for(unsigned i=0;i<32;++i){assert(native_scan_encode_row(&frame,i+1,8,i,&row));assert(native_scan_consume(&scan,&frame)==1);}
    assert(native_scan_encode_done(&frame,40,8,32,0));assert(native_scan_consume(&scan,&frame)==2 && scan.count==32);
    assert(!native_scan_begin(&scan,9,32));
    assert(native_scan_encode_row(&frame,1,9,1,&row));assert(native_scan_consume(&scan,&frame)==1);
    assert(native_scan_encode_done(&frame,2,9,1,0));assert(native_scan_consume(&scan,&frame)<0 && !scan.complete && !scan.received);
    assert(!native_scan_begin(&scan,10,1));assert(native_scan_encode_row(&frame,1,10,0,&row));frame.payload[46]=1;
    assert(native_scan_consume(&scan,&frame)<0 && scan.error==NATIVE_SCAN_BAD_WIRE);
    assert(!native_scan_begin(&scan,11,32));assert(native_scan_encode_row(&frame,1,11,0,&row));assert(native_scan_consume(&scan,&frame)==1);
    frame.payload[12]='x';assert(native_scan_consume(&scan,&frame)<0 && !scan.rows[0].ssid_length);
    row.ssid_length=33;assert(!native_scan_encode_row(&frame,1,1,0,&row));
    assert(!native_scan_encode_done(&frame,1,1,33,0));assert(!native_scan_encode_done(&frame,1,1,1,NATIVE_SCAN_BUSY));
    puts("PASS: real scan wire validation, stale replies, duplicates, bounded32 rows, missing rows and failure clearing");
}
