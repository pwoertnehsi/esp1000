#ifndef BROWSER_LOOKUP_H
#define BROWSER_LOOKUP_H
int browser_lookup_run(int (*report)(const char *,int),int (*cancelled)(void));
/* Refuse teardown while either native lookup or cancellation still runs. */
int browser_lookup_cleanup(void);
#endif
