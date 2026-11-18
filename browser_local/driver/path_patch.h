#ifndef PSP_BROWSER_PATH_PATCH_H
#define PSP_BROWSER_PATH_PATCH_H
#include <stddef.h>
typedef struct { unsigned offset; const char *original,*redirected; } browser_path_t;
/* All-or-nothing preflight, then write only the equal-length eight-byte prefix.
 * Caller owns synchronization/cache maintenance and verifies module identity. */
int browser_paths_change(unsigned char *segment,size_t size,const browser_path_t *paths,
                         unsigned count,int restore);
#endif
