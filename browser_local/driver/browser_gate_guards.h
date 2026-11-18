/* Browser-only E1000 rejection branches; delay slots preserved. */
static const struct {unsigned offset,branch,delay;} browser_gate_entries[]={
    {0x75cu,0x1062018eu,0x00808821u},
    {0x1584u,0x10830040u,0x8fbf0034u},
    {0x1830u,0x10830012u,0x00000000u},
    {0x1d1cu,0x10830007u,0x24020004u},
};
