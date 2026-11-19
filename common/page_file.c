#include <pspkernel.h>
#include <pspiofilemgr.h>
#include "page_file.h"

#ifndef PART_PATH
#define PART_PATH "ms0:/pspdev.html.part"
#endif
#ifndef BACKUP_PATH
#define BACKUP_PATH "ms0:/pspdev.html.bak"
#endif
static SceUID output = -1;

static int exists(const char *path)
{
    SceIoStat info;
    return sceIoGetstat(path, &info) >= 0;
}

static void abort_file(void *arg)
{
    (void)arg;
    if (output >= 0) sceIoClose(output);
    output = -1;
    sceIoRemove(PART_PATH);
}

static int begin_file(void *arg)
{
    abort_file(arg);
    /* Recover an old completed file if a previous publish was interrupted. */
    if (!exists(PAGE_PATH) && exists(BACKUP_PATH) &&
        sceIoRename(BACKUP_PATH, PAGE_PATH) < 0) return -1;
    output = sceIoOpen(PART_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    return output < 0 ? -1 : 0;
}

static int write_file(void *arg, const void *buffer, unsigned length)
{
    const unsigned char *p = buffer;
    (void)arg;
    while (length) {
        int n = sceIoWrite(output, p, length);
        if (n <= 0) return -1;
        length -= n;
        p += n;
    }
    return 0;
}

static int commit_file(void *arg)
{
    (void)arg;
    int r = sceIoSync("ms0:", 0);
    int close_result = sceIoClose(output);
    output = -1;
    if (r < 0 || close_result < 0) return -1;
    int previous = exists(PAGE_PATH);
    if (previous) {
        /* Never overwrite a recovery copy left by an earlier failed publish. */
        if (exists(BACKUP_PATH) || sceIoRename(PAGE_PATH, BACKUP_PATH) < 0) return -1;
    }
    if (sceIoRename(PART_PATH, PAGE_PATH) < 0) {
        if (previous) sceIoRename(BACKUP_PATH, PAGE_PATH);
        return -1;
    }
    if (sceIoSync("ms0:", 0) < 0) return -1;
    if (previous) sceIoRemove(BACKUP_PATH);
    return 0;
}

const link_file_ops_t page_file_ops = {
    .begin = begin_file, .write = write_file, .commit = commit_file, .abort = abort_file,
};
