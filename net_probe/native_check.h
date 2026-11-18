#ifndef PSP_NATIVE_CHECK_H
#define PSP_NATIVE_CHECK_H
/* Logger returns negative if recording/syncing failed. Never logs credentials.
 * Return -2 if native cleanup failed: do not unload its backing modules. */
int native_socket_check(int (*report)(const char *step, int result));
#endif
