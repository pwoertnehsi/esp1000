#include "path_patch.h"
#include <string.h>
int browser_paths_change(unsigned char *segment,size_t size,const browser_path_t *paths,
                         unsigned count,int restore)
{
    for(unsigned i=0;i<count;++i) {
        const char *from=restore?paths[i].redirected:paths[i].original;
        const char *to=restore?paths[i].original:paths[i].redirected;
        size_t n=strlen(from)+1;
        if(n<9 || strlen(to)+1!=n || memcmp(from+8,to+8,n-8)
           || paths[i].offset>size || n>size-paths[i].offset
           || memcmp(segment+paths[i].offset,from,n))return -1;
    }
    for(unsigned i=0;i<count;++i)
        memcpy(segment+paths[i].offset,restore?paths[i].original:paths[i].redirected,8);
    return 0;
}
