#ifndef WIFI_MENU_PATCH_H
#define WIFI_MENU_PATCH_H
#include <stdint.h>
#include <string.h>
#include "menu_guards.h"
#define MENU_LENGTH(a) (sizeof(a)/sizeof((a)[0]))
/* No call-site ownership assumptions: ARK can install its own call hooks after
 * module prologue. Its eventual call into the original action entry reaches us.
 * The displaced entry words are position independent stack-frame instructions. */
static int menu_patch_prepare(const uint32_t *text,unsigned size,uint32_t base,
    const uint32_t callbacks[2],const unsigned char *data,unsigned data_size,
    const uint32_t trampoline[4],uint32_t trampoline_address)
{
    if(size!=WIFI_VSH_TEXT_SIZE || (base&3))return -1;
    if(!data || data_size<WIFI_MENU_DATA_DIGEST_OFFSET+sizeof(menu_digest_original) ||
        memcmp(data+WIFI_MENU_DATA_DIGEST_OFFSET,menu_digest_original,sizeof(menu_digest_original)))return -6;
    if(memcmp(text+WIFI_MENU_TABLE_OFFSET/4,menu_visibility_original,sizeof(menu_visibility_original)))return -7;
    for(unsigned i=0;i<MENU_LENGTH(menu_anchors);++i)
        if(text[menu_anchors[i].offset/4]!=menu_anchors[i].word)return -2;
    if(text[WIFI_MENU_COLUMN_BRANCH/4]!=WIFI_MENU_COLUMN_ORIGINAL)return -20;
    for(unsigned i=0;i<MENU_LENGTH(menu_column_anchors);++i)
        if(text[menu_column_anchors[i].offset/4]!=menu_column_anchors[i].word)return -20;
    for(unsigned i=0;i<MENU_LENGTH(menu_resources);++i)
        if(memcmp(text+menu_resources[i].offset/4,menu_resources[i].words,20))return -8;
    if(memcmp(text+WIFI_MENU_ACTION_ENTRY/4,menu_entry_original,8))return -9;
    if(!trampoline || memcmp(trampoline,menu_trampoline_empty,16))return -16;
    uint32_t pc=base+WIFI_MENU_ACTION_ENTRY;
    if((callbacks[0]&3) || (trampoline_address&3) ||
        ((callbacks[0]^(pc+4))&0xf0000000u) ||
        (((trampoline_address+12)^(pc+8))&0xf0000000u))return -4;
    if(callbacks[0]==pc || trampoline_address==pc)return -5;
    return 0;
}
static void menu_patch_apply(uint32_t *text,unsigned char *data,const uint32_t callbacks[2],
    uint32_t trampoline[4],uint32_t base)
{
    trampoline[0]=menu_entry_original[0];trampoline[1]=menu_entry_original[1];
    trampoline[2]=0x08000000u|(((base+WIFI_MENU_ACTION_ENTRY+8)>>2)&0x03ffffffu);trampoline[3]=0;
    text[WIFI_MENU_ACTION_ENTRY/4]=0x08000000u|((callbacks[0]>>2)&0x03ffffffu);
    text[WIFI_MENU_ACTION_ENTRY/4+1]=0;
    /* Model10 otherwise calls the native column-count setter with count-2,
     * removing Network/PSN after their items/resources have been constructed. */
    text[WIFI_MENU_COLUMN_BRANCH/4]=0;
    for(unsigned i=0;i<MENU_LENGTH(menu_rows);++i)
        text[(menu_rows[i].offset+8)/4]=menu_rows[i].mask|0x400u;
    for(unsigned i=0;i<MENU_LENGTH(menu_resources);++i)
        text[(menu_resources[i].offset+12)/4]=menu_resources[i].words[3]|0x400u;
    memcpy(data+WIFI_MENU_DATA_DIGEST_OFFSET,menu_digest_modified,sizeof(menu_digest_modified));
}
static int menu_patch_verify(const uint32_t *text,const unsigned char *data,const uint32_t callbacks[2],
    const uint32_t trampoline[4],uint32_t base)
{
    if(memcmp(data+WIFI_MENU_DATA_DIGEST_OFFSET,menu_digest_modified,20))return -14;
    if(text[WIFI_MENU_COLUMN_BRANCH/4])return -21;
    for(unsigned i=0;i<MENU_LENGTH(menu_visibility_original);++i) {
        uint32_t expected=menu_visibility_original[i];
        for(unsigned j=0;j<MENU_LENGTH(menu_rows);++j)
            if(WIFI_MENU_TABLE_OFFSET+4*i==menu_rows[j].offset+8)expected|=0x400u;
        if(text[WIFI_MENU_TABLE_OFFSET/4+i]!=expected)return -14;
    }
    for(unsigned i=0;i<MENU_LENGTH(menu_resources);++i)for(unsigned j=0;j<5;++j) {
        uint32_t expected=menu_resources[i].words[j]|(j==3?0x400u:0);
        if(text[menu_resources[i].offset/4+j]!=expected)return -17;
    }
    if(text[WIFI_MENU_ACTION_ENTRY/4]!=(0x08000000u|((callbacks[0]>>2)&0x03ffffffu)) ||
        text[WIFI_MENU_ACTION_ENTRY/4+1]!=0)return -18;
    if(trampoline[0]!=menu_entry_original[0] || trampoline[1]!=menu_entry_original[1] ||
        trampoline[2]!=(0x08000000u|(((base+WIFI_MENU_ACTION_ENTRY+8)>>2)&0x03ffffffu)) || trampoline[3])return -19;
    return 2;
}
#endif
