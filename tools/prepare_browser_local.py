#!/usr/bin/env python3
"""Generate scoped path guards and a private, local memory-stick browser folder.
Sony assets come exclusively from this user's verified 6.61 extraction. The
code-only ZIP excludes them; never distribute the generated private asset directories.
"""
from pathlib import Path
import hashlib
import json
import shutil
import zlib
from inspect_prx import Prx
from prepare_browser_signed import signed_assets
ROOT=Path(__file__).resolve().parents[1]
MISSING=['htmlviewer_ui.prx','netfront.prx','libslim.prx','libwww.prx','mm_flash.prx']

def guards():
    output=[]
    for name in ['utility','htmlviewer_utility']:
        p=Prx(ROOT/'modules'/f'{name}.prx',{});report=p.report();segment=p.segments[0]
        blob=p.data[segment['offset']:segment['offset']+segment['filesz']]
        paths=[f'flash0:/vsh/module/{n}' for n in MISSING] if name=='utility' else ['flash0:/vsh/resource/']
        rows=[]
        for original in paths:
            needle=original.encode()+b'\0'
            if blob.count(needle)!=1:raise ValueError(f'Unexpected path count: {name}/{original}')
            redirected='brows0:/'+original[len('flash0:/'):]
            assert len(redirected)==len(original)
            rows.append({'offset':blob.index(needle),'original':original,'redirected':redirected})
        # Non-relocated prologue anchors; exclude jumps, LUI and instruction
        # words that can carry address relocations. The exact path bytes also
        # must match before the transaction writes anything.
        start=0x236c if name=='utility' else next(int(e['address'],16) for l in report['exports'] for e in l['entries'] if e['nid']=='0xD632ACDB')
        anchors=[]
        for offset in range(start,start+96,4):
            word=p.word(offset);op=word>>26
            if op in (0x23,0x2b) or (op==9 and (word>>21)&31==29):
                anchors.append((offset,word))
            if len(anchors)==4:break
        if len(anchors)!=4:raise ValueError('Insufficient stable anchors')
        output.append({'module':report['module'],'version':report['module_version_bytes'],
                       'text_size':segment['filesz'],'sha256':report['sha256'],'paths':rows,'anchors':anchors})
    lines=['/* Generated from local 6.61 PRXs; selected guards, not a full runtime hash. */',
           '#ifndef PSP_BROWSER_GUARDS_H','#define PSP_BROWSER_GUARDS_H']
    for i,mod in enumerate(output):
        lines.append(f'static const browser_path_t browser_paths_{i}[] = {{')
        for row in mod['paths']:lines.append(f'    {{0x{row["offset"]:x}u, "{row["original"]}", "{row["redirected"]}"}},')
        lines+=['};',f'static const browser_anchor_t browser_anchors_{i}[] = {{']
        for offset,word in mod['anchors']:lines.append(f'    {{0x{offset:x}u,0x{word:08x}u}},')
        lines.append('};')
    lines.append('static const browser_guard_t browser_guards[] = {')
    for i,m in enumerate(output):
        lines.append('    {"%s",%du,%du,0x%xu,browser_paths_%d,%du,browser_anchors_%d,4u},' %
                     (m['module'],*m['version'],m['text_size'],i,len(m['paths']),i))
    lines+=['};','#endif','']
    (ROOT/'browser_local/driver/browser_guards.h').write_text('\n'.join(lines))
    (ROOT/'analysis/browser_path_guards.json').write_text(json.dumps(output,indent=2)+'\n')
    p=Prx(ROOT/'modules/htmlviewer_utility.prx',{})
    lines=['/* Browser-only E1000 rejection branches; delay slots preserved. */',
           'static const struct {unsigned offset,branch,delay;} browser_gate_entries[]={']
    for offset in [0x75c,0x1584,0x1830,0x1d1c]:
        branch=p.word(offset);delay=p.word(offset+4)
        if branch>>26!=4:raise ValueError('Unexpected browser model gate')
        lines.append(f'    {{0x{offset:x}u,0x{branch:08x}u,0x{delay:08x}u}},')
    lines+=['};','']
    (ROOT/'browser_local/driver/browser_gate_guards.h').write_text('\n'.join(lines))

def apctl_guards():
    p=Prx(ROOT/'modules/pspnet_apctl.prx',{});r=p.report()
    nids=[0x5deac81b,0x2befdf23,0xcfb957c6,0x24fe91a1,0x8abadd51,0x5963991b]
    exports={int(e['nid'],16):int(e['address'],16) for lib in r['exports'] if lib['library']=='sceNetApctl' for e in lib['entries']}
    lines=[f"/* Generated 6.61 APCTL entry guards; SHA256 {r['sha256']} */",
           f"#define BROWSER_APCTL_SIZE 0x{p.segments[0]['filesz']:x}u",
           'static const struct {unsigned nid,offset,first,second;} apctl_entries[]={']
    for nid in nids:
        offset=exports[nid];first=p.word(offset);second=p.word(offset+4)
        if first>>26!=9 or second>>26!=0x2b:raise ValueError('Unexpected APCTL entry prologue')
        lines.append(f'    {{0x{nid:08x}u,0x{offset:x}u,0x{first:08x}u,0x{second:08x}u}},')
    lines+=['};','']
    (ROOT/'browser_local/driver/apctl_guards.h').write_text('\n'.join(lines))

def package():
    matrix=json.loads((ROOT/'analysis/browser_asset_matrix.json').read_text())
    signed=signed_assets()
    wanted={f'flash0:/vsh/module/{n}' for n in MISSING}|{'flash0:/vsh/resource/htmlviewer.res'}
    destination=ROOT/'build/browser-local'
    assets=[]
    for a in matrix['assets']:
        if a['firmware_path'] not in wanted:continue
        name=Path(a['firmware_path']).name
        record=signed[name] if name in signed else a
        source=ROOT/record['source'];data=source.read_bytes()
        if hashlib.sha256(data).hexdigest()!=record['sha256']:raise ValueError('Asset provenance mismatch')
        relative=Path('PSP/GAME/psp-browser')/('modules' if name in signed else 'resources')/name
        target=destination/relative
        if target.exists() and target.read_bytes()!=data:raise ValueError('Refusing to overwrite different runtime asset')
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        assets.append({'path':'ms0:/'+relative.as_posix(),'bytes':len(data),'crc32':zlib.crc32(data),'sha256':record['sha256']})
    if len(assets)!=6:raise ValueError('Incomplete browser asset matrix')
    lines=['#ifndef PSP_BROWSER_ASSETS_H','#define PSP_BROWSER_ASSETS_H',
           'static const struct {const char *path; unsigned bytes,crc;} browser_assets[] = {']
    for a in assets:lines.append(f'    {{"{a["path"]}",{a["bytes"]}u,0x{a["crc32"]:08x}u}},')
    lines+=['};','#endif','']
    (ROOT/'browser_local/app/assets.h').write_text('\n'.join(lines))
    (destination/'assets.json').write_text(json.dumps(assets,indent=2)+'\n')
    app=destination/'PSP/GAME/psp-browser';app.mkdir(parents=True,exist_ok=True)
    shutil.copy2(ROOT/'virtual_net/driver/psp_vnet.prx',app/'psp_vnet.prx') if (ROOT/'virtual_net/driver/psp_vnet.prx').exists() else None
    for source in ['app/EBOOT.PBP','driver/browser_paths.prx','README.txt','offline.html']:
        p=ROOT/'browser_local'/source
        if p.exists():shutil.copy2(p,app/p.name)
    print('Private local copy folder:',destination)

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets-only',action='store_true',help='Stage verified runtime assets without regenerating historical utility guards')
    args=parser.parse_args()
    if not args.assets_only:guards();apctl_guards()
    package()
