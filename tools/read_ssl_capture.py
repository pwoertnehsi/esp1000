#!/usr/bin/env python3
"""Explain metadata-only PSP SSL capture records; no UART connection required."""
import argparse
import re
VERSIONS={0x300:'SSL 3.0',0x301:'TLS 1.0',0x302:'TLS 1.1',0x303:'TLS 1.2',0x304:'TLS 1.3'}
ALERTS={0:'close_notify',10:'unexpected_message',20:'bad_record_mac',40:'handshake_failure',42:'bad_certificate',43:'unsupported_certificate',44:'certificate_revoked',45:'certificate_expired',46:'certificate_unknown',47:'illegal_parameter',48:'unknown_ca',49:'access_denied',50:'decode_error',51:'decrypt_error',70:'protocol_version',71:'insufficient_security',80:'internal_error',90:'user_canceled',112:'unrecognized_name'}
PATTERN=re.compile(r'SSL_TRACE time_us=(\d+) flow=(\d+) peer=([\d.]+) local=(\d+) remote=(\d+) dir=(TX|RX) event=(\w+) a=([0-9a-f]+) b=([0-9a-f]+) c=([0-9a-f]+)')
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('log');args=parser.parse_args()
    session=0
    with open(args.log,encoding='utf-8',errors='replace') as source:
        for line in source:
            if line.startswith('SSL_CAPTURE BEGIN'):
                session+=1;print(f'Capture {session}');continue
            if line.startswith('SSL_CAPTURE'):
                print(line.strip());continue
            match=PATTERN.search(line)
            if not match:continue
            time,flow,peer,local,remote,direction,kind,*values=match.groups()
            a,b,c=(int(x,16) for x in values);detail=f'a={a:#x} b={b:#x} c={c:#x}'
            if kind in ('CLIENT_HELLO','SSL2_CLIENT_HELLO'):
                detail=f'offered={VERSIONS.get(a,hex(a))} cipher_count={b}'
                if kind=='CLIENT_HELLO':detail+=f' SNI={bool(c&1)} supported_versions_extension={bool(c&2)}'
            elif kind=='SERVER_HELLO':detail=f'legacy_version={VERSIONS.get(a,hex(a))} cipher={b:#06x}'
            elif kind=='VERSIONS':detail=f'highest_listed_version={VERSIONS.get(a,hex(a))} count={b}'
            elif kind=='CIPHER':detail=f'cipher[{b}]={a:#06x} (first eight of {c})'
            elif kind=='ALERT':detail=f'level={a} description={b} ({ALERTS.get(b,"unknown")}) record_version={VERSIONS.get(c,hex(c))}'
            elif kind in ('RECORD','ENCRYPTED'):detail=f'type={a} record_version={VERSIONS.get(b,hex(b))} bytes={c}'
            elif kind=='HANDSHAKE':detail=f'type={a} bytes={b}'
            elif kind=='END':detail=f'new_tcp_bytes TX={a} RX={b} decode_disabled_mask={c}'
            elif kind=='LIMIT':detail='metadata event budget reached; later handshake details may be missing'
            elif kind=='TCP_GAP':detail='missing/out-of-order TCP bytes; this direction will no longer be decoded'
            print(f'{time}us flow={flow} {direction} {peer}:{remote} local={local} {kind}: {detail}')
if __name__=='__main__':main()
