#include "code_patch.h"
int browser_code_change(browser_code_patch_t *rows,unsigned count,int restore)
{
    for(unsigned i=0;i<count;++i) {
        const uint32_t *expected=restore?rows[i].replacement:rows[i].original;
        if(!rows[i].address || rows[i].address[0]!=expected[0] || rows[i].address[1]!=expected[1])return -1;
    }
    for(unsigned i=0;i<count;++i) {
        const uint32_t *value=restore?rows[i].original:rows[i].replacement;
        rows[i].address[0]=value[0];rows[i].address[1]=value[1];
    }
    return 0;
}
