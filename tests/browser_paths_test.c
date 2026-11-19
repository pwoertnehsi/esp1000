#include "../browser_local/driver/path_patch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    unsigned char memory[160],before[160];memset(memory,0xa5,sizeof(memory));
    const browser_path_t paths[]={{8,"flash0:/vsh/module/a.prx","brows0:/vsh/module/a.prx"},{80,"flash0:/vsh/resource/","brows0:/vsh/resource/"}};
    for(unsigned i=0;i<2;++i)strcpy((char *)memory+paths[i].offset,paths[i].original);
    memcpy(before,memory,sizeof(memory));
    assert(!browser_paths_change(memory,sizeof(memory),paths,2,0));
    assert(!strcmp((char *)memory+8,paths[0].redirected) && !strcmp((char *)memory+80,paths[1].redirected));
    assert(browser_paths_change(memory,sizeof(memory),paths,2,0)==-1);
    assert(!browser_paths_change(memory,sizeof(memory),paths,2,1) && !memcmp(memory,before,sizeof(memory)));
    memory[80]='X';unsigned char corrupted[160];memcpy(corrupted,memory,sizeof(memory));
    assert(browser_paths_change(memory,sizeof(memory),paths,2,0)==-1 && !memcmp(memory,corrupted,sizeof(memory)));
    memcpy(memory,before,sizeof(memory));
    browser_path_t bad={(unsigned)-1,paths[0].original,paths[0].redirected};
    assert(browser_paths_change(memory,sizeof(memory),&bad,1,0)==-1 && !memcmp(memory,before,sizeof(memory)));
    bad=paths[0];bad.redirected="brows0:/vsh/module/x.prx";
    assert(browser_paths_change(memory,sizeof(memory),&bad,1,0)==-1 && !memcmp(memory,before,sizeof(memory)));
    assert(!browser_paths_change(memory,sizeof(memory),paths,2,0));memory[80]='X';memcpy(corrupted,memory,sizeof(memory));
    assert(browser_paths_change(memory,sizeof(memory),paths,2,1)==-1 && !memcmp(memory,corrupted,sizeof(memory)));
    puts("PASS: scoped path transactions, bounds, only-prefix changes, rollback and foreign edits preserved");
}
