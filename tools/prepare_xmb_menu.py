#!/usr/bin/env python3
"""Recover numeric menu masks and exact labels from the owned 6.61 VSH."""
from pathlib import Path
import json
import hashlib
import struct
import zlib
from inspect_prx import Prx
ROOT=Path(__file__).resolve().parents[1]
p=Prx(ROOT/'modules/vshmain.prx',{})
report=p.report()
# Code 207a4/207a8 selects this 35-row (category,id,model-mask,region-mask)
# table; 20e98 gates model bits, 20ee0..20f00 selects id*80+data-segment018c.
base=0x54b20
rows=[]
for i in range(35):
    address=base+i*16
    cat,item,mask,region=[p.word(address+j) for j in (0,4,8,12)]
    desc=p.segments[1]['vaddr']+0x18c+80*item
    data=p.read(p.file_offset(desc,80),80)
    label=data[43:].split(b'\0')[0].decode('ascii')
    restore=cat in (6,7) or label in ('msgtop_sysconf_network','msgtop_sysconf_rss','msgtop_game_gamedl')
    rows.append(dict(offset=address,category=cat,item=item,mask=mask,region=region,label=label,
                     action=p.word(desc+8),argument=p.word(desc+12),restore=restore))
anchors=[(a,p.word(a)) for a in (0x20654,0x20658,0x207d0,0x20e98,0x20ee0,0x20ef0,0x22648,0x16a70,0x16a48,0x16a50,0x308b0,0x308b8,0x1fec4,0x1fee0,0x1fee4)]
lines=['/* Generated from owned 6.61 vshmain.prx. Numeric masks only; region masks retained. */',
       '#define WIFI_VSH_TEXT_SIZE %du'%p.segments[0]['filesz'],
       'static const struct {unsigned offset,word;} menu_anchors[]={']
lines += ['    {0x%xu,0x%08xu},'%x for x in anchors]
lines += ['};','static const struct {unsigned offset,category,item,mask,region;} menu_rows[]={']
for r in rows:
    if r['restore']:
        lines.append('    {0x%xu,%du,%du,0x%xu,0x%xu}, /* %s */'%(r['offset'],r['category'],r['item'],r['mask'],r['region'],r['label']))
lines += ['};','static const unsigned menu_call_offsets[]={0x16a4cu,0x308b4u};','']
original=p.read(p.file_offset(base,560),560)
embedded=p.read(p.file_offset(p.segments[1]['vaddr']+0x10,20),20)
fallback=zlib.decompress(p.read(p.file_offset(p.segments[1]['vaddr']+0x30,348),348))
if hashlib.sha1(original).digest()!=embedded or fallback!=original:
    raise ValueError('Native visibility integrity record does not match the table')
modified=bytearray(original)
for row in rows:
    if row['restore']:struct.pack_into('<I',modified,row['offset']-base+8,row['mask']|0x400)
lines += ['#define WIFI_MENU_TABLE_OFFSET 0x54b20u',
          '#define WIFI_MENU_DATA_DIGEST_OFFSET 0x10u',
          'static const uint32_t menu_visibility_original[]={']
for row in rows:
    lines.append('    %du,%du,0x%xu,0x%xu,'%(row['category'],row['item'],row['mask'],row['region']))
lines += ['};']
for name,digest in [('original',embedded),('modified',hashlib.sha1(modified).digest())]:
    lines += ['static const unsigned char menu_digest_%s[20]={%s};'%(name,','.join('0x%02x'%x for x in digest))]

# Resource masks are separate from item visibility. Resolve exactly the three
# icon keys stored by each restored descriptor; do not enable unrelated icons.
resource_rows=[]
for row in rows:
    if not row['restore']:continue
    desc=p.segments[1]['vaddr']+0x18c+80*row['item']
    raw=p.read(p.file_offset(desc,80),80)
    keys=tuple(struct.unpack_from('<I',raw,offset)[0] for offset in (31,35,39))
    matches=[]
    for start,count in [(0x54d50,8),(0x54df0,20),(0x54f80,14),(0x55098,8)]:
        for index in range(count):
            address=start+20*index
            values=[p.word(address+j) for j in (0,4,8,12,16)]
            if tuple(values[:3])==keys:matches.append((address,values))
    if len(matches)!=1:raise ValueError('Ambiguous icon resources: '+row['label'])
    address,values=matches[0]
    resource_rows.append(dict(offset=address,words=values,item=row['item'],label=row['label']))
lines += ['#define WIFI_MENU_ACTION_ENTRY 0x16a70u',
          'static const uint32_t menu_entry_original[2]={0x27bdffe0u,0xafb3000cu};',
          'static const uint32_t menu_trampoline_empty[4]={0x03e00008u,0x2402ffffu,0u,0u};',
          'static const struct {unsigned offset;uint32_t words[5];} menu_resources[]={']
for row in resource_rows:
    lines.append('    {0x%xu,{%s}}, /* %s */'%(row['offset'],','.join('0x%xu'%x for x in row['words']),row['label']))
lines += ['};',
          '#define WIFI_MENU_COLUMN_BRANCH 0x20c20u',
          '#define WIFI_MENU_COLUMN_ORIGINAL 0x50620029u',
          'static const struct {unsigned offset,word;} menu_column_anchors[]={']
for a in (0x20c1c,0x20c24,0x20c28,0x20cc8,0x20cd0):
    lines.append('    {0x%xu,0x%08xu},'%(a,p.word(a)))
lines += ['};','']
if p.word(0x20c20)!=0x50620029:raise ValueError('Unexpected model10 column pruning branch')

for addr in [0x16a4c,0x308b4]:
    if p.word(addr)!=0x0c005a9c:raise ValueError('Unexpected action call')
(ROOT/'wifi_plugin/menu_guards.h').write_text('\n'.join(lines))
(ROOT/'analysis/xmb_menu_catalog.json').write_text(json.dumps({'module':report['module'],'source_sha256':report['sha256'],'rows':rows,'anchors':anchors,'resources':resource_rows},indent=2)+'\n')
print('Recovered native menu masks, labels and action routing')
