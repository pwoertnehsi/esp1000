/* Owned 6.61 HTML viewer close sequence. Preserve every argument/delay slot. */
static const struct {unsigned call,target,delay;} browser_exit_calls[]={
    {0x29c4,0xd7a8,0xa0450680},{0x29cc,0xd834,0},
    {0x29d8,0x1c3c,0x02002021},{0x29e4,0x1bd4,0x24050008},
    {0x29ec,0x4cec,0x00002021},{0x29f4,0x4c90,0x00002021}
};
static unsigned exit_jal(unsigned address)
{return 0x0c000000u|((address>>2)&0x03ffffffu);}
static int browser_exit_preflight(const unsigned *text,unsigned base,
    const uint32_t callbacks[WIFI_EXIT_STAGES],int ours)
{
    if(text[0x296c/4]!=0x27bdfff0 || text[0x29fc/4]!=(exit_jal(base+0x299c)^0x04000000u) || text[0x2a00/4]!=0x8fbf0004)return -1;
    for(unsigned i=0;i<WIFI_EXIT_STAGES;++i) {
        unsigned at=browser_exit_calls[i].call;
        if((callbacks[i]&3) || ((callbacks[i]^(base+at+4))&0xf0000000u))return -2;
        unsigned target=ours?callbacks[i]:base+browser_exit_calls[i].target;
        if(text[at/4]!=exit_jal(target) || text[at/4+1]!=browser_exit_calls[i].delay)return -3;
    }
    return 0;
}
