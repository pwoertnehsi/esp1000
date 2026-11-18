#ifndef BROWSER_CODE_PATCH_H
#define BROWSER_CODE_PATCH_H
#include <stdint.h>
typedef struct {uint32_t *address,original[2],replacement[2];} browser_code_patch_t;
int browser_code_change(browser_code_patch_t *rows,unsigned count,int restore);
#endif
